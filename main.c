#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <openssl/sha.h>
#include "bencoder.h"
#include "bencode_parser.h"
#include "helpers.h"
#include "announce_connector.h"
#include "handshake.h"
#include "downloader.h"
#include "file_saver.h"

#define DEFAULT_BLOCK_SIZE 16384

// TODO: handle edge cases https://en.wikipedia.org/wiki/Bencode

int main() {
    const char *fileName = "./../imaginedragons.torrent";
    BencodeContext ctx;
    ctx.file = fopen(fileName, "rb");
    ctx.hasError = false;
    ctx.errorPosition = 0;
    memset(ctx.errorMsg, 0, sizeof(ctx.errorMsg));

    if (!ctx.file) {
        perror("Error opening file");
        return 1;
    }

    const int ch = fpeek(ctx.file);
    if (ch == EOF) {
        perror("File is empty.");
        return 1;
    }

    BencodeNode *root = parseDict(&ctx);
    if (ctx.hasError) {
        printf("Error: %s\n", ctx.errorMsg);
        printf("Position: %ld (0x%lX)\n", ctx.errorPosition, ctx.errorPosition);

        if (root) {
            freeBencodeNode(root);
        }
    }

    if (!root) {
        printf("Failed to extract content of .torrent file.");
        fclose(ctx.file);
        return 0;
    }

    BencodeNode *infoNode = getDictValue(root, "info");
    if (!infoNode) {
        printf("Failed to extract value of \"info\" from the file.");
        return 0;
    }

    BencodeNode *pieces_node = getDictValue(infoNode, "pieces");
    BencodeNode *piece_length_node = getDictValue(infoNode, "piece length");

    if (!pieces_node || pieces_node->type != BEN_STR || pieces_node->string.length == 0 || !piece_length_node ||
        piece_length_node->type != BEN_INT) {
        puts("Failed to extract value of \"pieces\" or \"piece info\" from the file.");
        freeBencodeNode(root);
        fclose(ctx.file);
        return -1;
    }

    const long infoLength = infoNode->endOffset - infoNode->startOffset;
    char *infoContent = malloc(infoLength);
    fseek(ctx.file, infoNode->startOffset, SEEK_SET);
    fread(infoContent, infoLength, 1, ctx.file);

    printf("Content: \n%s\n", infoContent);

    unsigned char info_hash[SHA_DIGEST_LENGTH];
    SHA1(infoContent, infoLength, info_hash);

    BencodeNode *announce_node = getDictValue(root, "announce");

    if (!announce_node || announce_node->type != BEN_STR || announce_node->string.length == 0) {
        printf("Invalid \"announce\".");
        free(infoContent);
        freeBencodeNode(root);
        fclose(ctx.file);
        return 0;
    }

    unsigned char peer_id[20];
    rand_str(peer_id, 20);

    UdpAnnounceRequest announce_data;
    announce_data.announce_address = announce_node->string.data;
    announce_data.info_hash = info_hash;
    announce_data.peer_id = peer_id;
    // announce_data.ip = ;
    announce_data.port = DEFAULT_ANNOUNCE_PORT;
    announce_data.uploaded = 0;
    announce_data.downloaded = 0;
    // announce_data.left = ; // TODO: probably need to pass total size?

    size_t peers_length = 0;
    char *peers = get_peers_list(&announce_data, &peers_length);

    if (!peers) {
        printf("Failed to connect to announce address. Attempting to connect to one of the addresses from announce-list.");
        const BencodeNode *announce_list_node = getDictValue(root, "announce-list");
        if (announce_list_node && announce_list_node->type == BEN_LIST) {
            for (int i = 0; i < announce_list_node->list.length; i++) {
                const BencodeNode *tier = announce_list_node->list.items[i];

                if (tier && tier->type == BEN_LIST) {
                    for (int j = 0; j < tier->list.length; j++) {
                        const BencodeNode *tracker_url = tier->list.items[j];

                        if (tracker_url && tracker_url->type == BEN_STR) {
                            announce_data.announce_address = tracker_url->string.data;

                            peers = get_peers_list(&announce_data, &peers_length);

                            if (peers) {
                                printf("Successfully retrieved peers from: %s\n", announce_data.announce_address);
                                break;
                            }
                        }
                    }
                }
                if (peers) {
                    break;
                }
            }
        }
    }

    if (!peers) {
        printf("No peers found or connection failed.\n");
        free(infoContent);
        freeBencodeNode(root);
        fclose(ctx.file);
        return -1;
    }
    const size_t count = peers_length / 6;
    printf("Received %lu peers:\n", count);

    for (int i = 0; i < count; i++) {
        const unsigned char *p = peers + i * 6;
        const int port = p[4] << 8 | p[5];
        printf("Peer %d: %d.%d.%d.%d:%d\n",
               i + 1,
               p[0], p[1], p[2], p[3],
               port);
    }

    bool* peer_inventory = NULL;
    const size_t total_pieces = pieces_node->string.length / SHA_DIGEST_LENGTH;
    const int active_peer_sockfd = establish_handshake(peers, count, info_hash, peer_id, total_pieces, &peer_inventory);

    if (active_peer_sockfd == -1) {
        printf("Failed to establish a handshake.\n");
        free(peers);
        free(infoContent);
        freeBencodeNode(root);
        fclose(ctx.file);
        return -1;
    }

    const unsigned char *pieces_hashes = pieces_node->string.data;
    const size_t piece_length = piece_length_node->intValue;

    size_t num_of_files = 0;
    EndFile *target_files = fill_target_files(infoNode, &num_of_files);

    if (!target_files) {
        printf("Failed to parse files from torrent.\n");
        close(active_peer_sockfd);
        free(peers);
        free(infoContent);
        freeBencodeNode(root);
        fclose(ctx.file);
        return -1;
    }

    const size_t total_torrent_size = target_files[num_of_files - 1].global_end;

    unsigned char *piece_buffer = malloc(piece_length);

    for (int current_piece = 0; current_piece < total_pieces; current_piece++) {
        if (peer_inventory && !peer_inventory[current_piece]) {
            continue;
        }
        size_t current_piece_size = piece_length;

        // if the piece is the last (size may differ)
        if (current_piece == total_pieces - 1) {
            const size_t remainder = total_torrent_size % piece_length;
            if (remainder != 0) {
                current_piece_size = remainder;
            }
        }

        const size_t number_of_blocks = (current_piece_size + DEFAULT_BLOCK_SIZE - 1) / DEFAULT_BLOCK_SIZE;

        printf("Downloading Piece %d / %zu (Size: %zu bytes)\n", current_piece, total_pieces - 1,
               current_piece_size);

        bool has_piece_downloaded_successfully = true;

        for (int i = 0; i < number_of_blocks; i++) {
            const uint32_t begin_offset = i * DEFAULT_BLOCK_SIZE;

            uint32_t block_size_to_request = DEFAULT_BLOCK_SIZE;
            // again, if the block is the last
            if (begin_offset + DEFAULT_BLOCK_SIZE > current_piece_size) {
                block_size_to_request = current_piece_size - begin_offset;
            }

            unsigned char *downloaded_block = download_block(active_peer_sockfd, current_piece, begin_offset,
                                                             block_size_to_request);
            if (!downloaded_block) {
                printf("Failed to download block at offset %u.\n", begin_offset);
                has_piece_downloaded_successfully = false;
                break;
            }

            memcpy(piece_buffer + begin_offset, downloaded_block, block_size_to_request);
            free(downloaded_block);
        }

        if (!has_piece_downloaded_successfully) {
            printf("Stopping the downloading. Peer connection lost.\n");
            break;
        }

        const unsigned char *expected_hash = pieces_hashes + current_piece * SHA_DIGEST_LENGTH;
        if (!verify_piece(piece_buffer, current_piece_size, expected_hash)) {
            puts("Failed to verify a piece.");
            break;
        }

        printf("Verified piece %d, will write to the disk.\n", current_piece);
        write_piece_to_disk(current_piece, piece_length, piece_buffer, target_files, num_of_files);
    }

    free(peer_inventory);
    free(piece_buffer);
    free(target_files);
    close(active_peer_sockfd);
    free(peers);
    free(infoContent);
    freeBencodeNode(root);
    fclose(ctx.file);
    return 0;
}
