#ifndef FILE_SAVER_H
#define FILE_SAVER_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char filepath[256];
    size_t length;
    size_t global_start;
    size_t global_end;
} EndFile;

void write_piece_to_disk(uint32_t piece_index, size_t piece_length, const unsigned char* piece_buffer, const EndFile* end_files, int num_files);
#endif // FILE_SAVER_H
