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
    }

    for (int i = 0; i < 10; i++) {
        const bool is_downloaded = download_piece(active_peer_sockfd, i);
        if (is_downloaded) {
            printf("Downloaded.\n");
        }
        else {
            printf("Not downloaded.\n");
        }
    }


    close(active_peer_sockfd);
    free(peers);
    free(infoContent);
    freeBencodeNode(root);
    fclose(ctx.file);
    return 0;
}
