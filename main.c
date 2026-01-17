#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <openssl/sha.h>
#include "bencoder.h"
#include "bencode_parser.h"
#include "helpers.h"
#include "announce_connector.h"

// TODO: handle edge cases https://en.wikipedia.org/wiki/Bencode

int main() {
    const char *fileName = "./../sometorrent.torrent";
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

    BencodeNode *root = NULL;

    if (ch == 'd') {
        fgetc(ctx.file);
        root = parseDict(&ctx);
    } else {
        reportError(&ctx, "File does not start with a dictionary (found '%c' instead).", ch);
    }

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


    get_peers_list(&announce_data);
    free(infoContent);
    freeBencodeNode(root);
    fclose(ctx.file);
    return 0;
}
