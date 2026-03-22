#include "file_saver.h"

#include <stdio.h>
#include <stdlib.h>

void write_piece_to_disk(const uint32_t piece_index, const size_t piece_length, const unsigned char *piece_buffer,
                         const EndFile *end_files, const int num_files) {
    const size_t piece_global_start = piece_index * piece_length;
    const size_t piece_global_end = piece_global_start + piece_length;
    size_t bytes_written_from_buffer = 0;

    for (int i = 0; i < num_files; i++) {
        const EndFile *file = &end_files[i];

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

EndFile *fill_target_files(const BencodeNode *infoNode, size_t *num_files) {
    const BencodeNode *single_file_length = getDictValue(infoNode, "length");
    EndFile *target_files = NULL;

    if (single_file_length) {
        *num_files = 1;
        target_files = malloc(sizeof(EndFile));

        const BencodeNode *name_node = getDictValue(infoNode, "name");
        snprintf(target_files[0].filepath, sizeof(target_files[0].filepath), "%s", name_node->string.data);
        target_files[0].length = single_file_length->intValue;
        target_files[0].global_start = 0;
        target_files[0].global_end = target_files[0].length;

        return target_files;
    }
    const BencodeNode *files_list = getDictValue(infoNode, "files");

    if (files_list && files_list->type == BEN_LIST) {
        *num_files = files_list->list.length;
        target_files = malloc(*num_files * sizeof(EndFile));

        size_t current_global_offset = 0;

        for (int i = 0; i < *num_files; i++) {
            const BencodeNode *file_dict = files_list->list.items[i];
            const BencodeNode *length_node = getDictValue(file_dict, "length");
            const BencodeNode *path_list = getDictValue(file_dict, "path");

            const BencodeNode *filename_node = path_list->list.items[path_list->list.length - 1];

            snprintf(target_files[i].filepath, sizeof(target_files[i].filepath), "%s", filename_node->string.data);
            target_files[i].length = length_node->intValue;
            target_files[i].global_start = current_global_offset;
            target_files[i].global_end = current_global_offset + target_files[i].length;

            current_global_offset = target_files[i].global_end;
        }
        return target_files;
    }
    perror("Invalid files dictionary in torrent.\n");
    return NULL;
}
