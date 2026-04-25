#include "downloader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/sha.h>

#define REQ_MSG_SIZE 17

unsigned char* download_block(const int sockfd, const uint32_t piece_index, const size_t begin_offset, const size_t block_size) {
    printf("Trying to download a piece: %d\n", piece_index);
    uint8_t request_msg[REQ_MSG_SIZE];

    const uint32_t req_len_prefix = htobe32(13);   // length of payload + id
    const uint8_t req_id = 6;                      // 6 = Request, TODO: remake to enum
    const uint32_t req_index = htobe32(piece_index);
    const uint32_t req_begin = htobe32(begin_offset);
    const uint32_t req_block_len = htobe32(block_size);

    memcpy(request_msg + 0, &req_len_prefix, 4);
    memcpy(request_msg + 4, &req_id, 1);
    memcpy(request_msg + 5, &req_index, 4);
    memcpy(request_msg + 9, &req_begin, 4);
    memcpy(request_msg + 13, &req_block_len, 4);

    if (send(sockfd, request_msg, 17, 0) != REQ_MSG_SIZE) {
        puts("Failed to send a request message.");
        return NULL;
    }

    while (true) {
        uint32_t reply_len_net;
        if (recv(sockfd, &reply_len_net, 4, MSG_WAITALL) <= 0) {
            printf("Connection dropped while waiting for block %lu data.\n", begin_offset);
            return NULL;
        }

        const uint32_t reply_len = be32toh(reply_len_net);
        // TODO: specify later in document that this way we handle keep alive msges
        if (reply_len == 0) {
            continue;
        }

        uint8_t reply_id;
        recv(sockfd, &reply_id, 1, MSG_WAITALL);

        if (reply_id == 7) { // 7 = PIECE, TODO: make enum
            uint32_t received_index, received_begin;
            recv(sockfd, &received_index, 4, MSG_WAITALL);
            recv(sockfd, &received_begin, 4, MSG_WAITALL);

            received_index = be32toh(received_index);
            received_begin = be32toh(received_begin);

            const uint32_t data_length = reply_len - 9; // -1 for id, -8 for metadata

            printf("Incoming block: %u bytes for piece %u at offset %u.\n", data_length, received_index, received_begin);

            unsigned char *file_data = malloc(data_length);
            const ssize_t bytes_read = recv(sockfd, file_data, data_length, MSG_WAITALL);

            if (bytes_read == data_length) {
                return file_data;
            }

            free(file_data);
            return NULL;
        }
        const uint32_t discard_len = reply_len - 1;
        unsigned char *discard = malloc(discard_len);
        recv(sockfd, discard, discard_len, MSG_WAITALL);
        free(discard);
    }
}

bool verify_piece(const unsigned char *piece_data, const size_t piece_length, const unsigned char *expected_hash) {
    unsigned char computed_hash[SHA_DIGEST_LENGTH];

    SHA1(piece_data, piece_length, computed_hash);

    if (memcmp(computed_hash, expected_hash, SHA_DIGEST_LENGTH) != 0) {
        puts("Failed to compute hash.");
        return false;
    }
    return true;
}
