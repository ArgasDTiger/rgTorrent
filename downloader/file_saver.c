#include "file_saver.h"

#include <stdio.h>

void write_piece_to_disk(const uint32_t piece_index, const size_t piece_length, const unsigned char* piece_buffer, const EndFile* end_files, const int num_files) {
    const size_t piece_global_start = piece_index * piece_length;
    const size_t piece_global_end = piece_global_start + piece_length;
    size_t bytes_written_from_buffer = 0;

    for (int i = 0; i < num_files; i++) {
        const EndFile* file = &end_files[i];

        // checks whether 2 files have overlapping piece of data, continues if not
        if (piece_global_end <= file->global_start || piece_global_start >= file->global_end) {
            continue;
        }

        // how many bytes to write to this file
        const size_t overlap_end = piece_global_end < file->global_end ? piece_global_end : file->global_end;
        const size_t overlap_start = piece_global_start > file->global_start ? piece_global_start : file->global_start;

        const size_t write_length = overlap_end - overlap_start;
        const size_t local_file_offset = overlap_start - file->global_start;

        FILE *f = fopen(file->filepath, "rb+");
        if (!f) {
            f = fopen(file->filepath, "wb+");
        }

        if (f) {
            fseek(f, local_file_offset, SEEK_SET);

            fwrite(piece_buffer + bytes_written_from_buffer, 1, write_length, f);
            fclose(f);
        }

        bytes_written_from_buffer += write_length;

        if (bytes_written_from_buffer >= piece_length) {
            break;
        }
    }
}
