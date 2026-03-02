#ifndef DOWNLOADER_H
#define DOWNLOADER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

unsigned char* download_block(int sockfd, uint32_t piece_index, size_t begin_offset, size_t block_size);

bool verify_piece(const unsigned char *piece_data, size_t piece_length, const unsigned char *expected_hash);
#endif // DOWNLOADER_H
