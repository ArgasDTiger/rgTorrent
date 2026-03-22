#ifndef FILE_SAVER_H
#define FILE_SAVER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bencoder.h"

typedef struct {
    char filepath[256];
    size_t length;
    size_t global_start;
    size_t global_end;
} EndFile;

void write_piece_to_disk(uint32_t piece_index, size_t piece_length, const unsigned char *piece_buffer,
                         const EndFile *end_files, int num_files);

void fill_target_files(const BencodeNode *infoNode, EndFile* target_files, size_t* num_files, bool *isSuccess);
#endif // FILE_SAVER_H
