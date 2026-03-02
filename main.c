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

#define DEFAULT_BLOCK_SIZE 16384

// TODO: handle edge cases https://en.wikipedia.org/wiki/Bencode

int main() {
    const char *fileName = "./../Fedora-Budgie-Live-x86_64-43.torrent";
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

    BencodeNode* infoNode = getDictValue(root, "info");
    if (!infoNode) {
        printf("Failed to extract value of \"info\" from the file.");
        return 0;
    }

    BencodeNode* pieces_node = getDictValue(infoNode, "pieces");
    BencodeNode* piece_length_node = getDictValue(infoNode, "piece length");

    if (!pieces_node || pieces_node->type != BEN_STR || pieces_node->string.length == 0 || !piece_length_node ||
        piece_length_node->type != BEN_INT) {
        puts("Failed to extract value of \"pieces\" or \"piece info\" from the file.");
        freeBencodeNode(root);
        fclose(ctx.file);
        return -1;
    }

    const long infoLength = infoNode->endOffset - infoNode->startOffset;
    char* infoContent = malloc(infoLength);
    fseek(ctx.file, infoNode->startOffset, SEEK_SET);
    fread(infoContent, infoLength, 1, ctx.file);

    printf("Content: \n%s\n", infoContent);

    unsigned char info_hash[SHA_DIGEST_LENGTH];
    SHA1(infoContent, infoLength, info_hash);

    BencodeNode* announce_node = getDictValue(root, "announce");

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


    size_t peers_length = 0;
    char* peers = get_peers_list(&announce_data, &peers_length);

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

    const int active_peer_sockfd = establish_handshake(peers, count, info_hash, peer_id);

    if (active_peer_sockfd == -1) {
        printf("Failed to establish a handshake.\n");
        free(peers);
        free(infoContent);
        freeBencodeNode(root);
        fclose(ctx.file);
        return -1;
    }

    const unsigned char* pieces_hashes = pieces_node->string.data;
    const size_t piece_length = piece_length_node->intValue;

    int target_piece_index = 0;
    unsigned char* piece = malloc(piece_length);
    const size_t number_of_blocks = piece_length / DEFAULT_BLOCK_SIZE;

    for (int i = 0; i < number_of_blocks; i++) {
        const uint32_t begin_offset = i * DEFAULT_BLOCK_SIZE;

        unsigned char* downloaded_block = download_block(active_peer_sockfd, target_piece_index, begin_offset, DEFAULT_BLOCK_SIZE);

        if (!downloaded_block) {
            printf("Failed to download block at offset %u.\n", begin_offset);
            free(piece);
            close(active_peer_sockfd);
            free(peers);
            free(infoContent);
            freeBencodeNode(root);
            fclose(ctx.file);
            return -1;
        }

        memcpy(piece + begin_offset, downloaded_block, DEFAULT_BLOCK_SIZE);

        free(downloaded_block);

        printf("Progress: %d / %lu blocks downloaded.\n", i + 1, number_of_blocks);
    }


    const unsigned char* expected_hash = pieces_hashes + 0 * SHA_DIGEST_LENGTH;
    if (!verify_piece(piece, piece_length, expected_hash)) {
        puts("Failed to verify a piece.");
        free(piece);
        close(active_peer_sockfd);
        free(peers);
        free(infoContent);
        freeBencodeNode(root);
        fclose(ctx.file);
        return -1;
    }


    free(piece);
    close(active_peer_sockfd);
    free(peers);
    free(infoContent);
    freeBencodeNode(root);
    fclose(ctx.file);
    return 0;
}
