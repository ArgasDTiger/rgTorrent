#include "file_saver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void create_parent_directories(const char *filepath) {
    char temp_path[1024];
    strncpy(temp_path, filepath, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';

    char *last_slash = strrchr(temp_path, '/');
    if (!last_slash) return;

    *last_slash = '\0';

    char *p = temp_path;
    if (*p == '/') p++;

    for (; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(temp_path, 0777);
            *p = '/';
        }
    }
    mkdir(temp_path, 0777);
}

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

        create_parent_directories(file->filepath);
        FILE *f = fopen(file->filepath, "rb+");
        if (!f) {
            f = fopen(file->filepath, "wb+");
        }

        if (f) {
            fseek(f, local_file_offset, SEEK_SET);
            fwrite(piece_buffer + bytes_written_from_buffer, 1, write_length, f);
            fclose(f);
        } else {
            fprintf(stderr, "[ERROR] Could not open/create file when writing to the disk: %s.\n", file->filepath);
        }

        bytes_written_from_buffer += write_length;

        if (bytes_written_from_buffer >= piece_length) {
            break;
        }
    }
}

EndFile *fill_target_files(const BencodeNode *infoNode, size_t *num_files, const char *save_path) {
    const BencodeNode *single_file_length = getDictValue(infoNode, "length");
    EndFile *target_files = NULL;

    if (single_file_length) {
        *num_files = 1;
        target_files = malloc(sizeof(EndFile));

        const BencodeNode *name_node = getDictValue(infoNode, "name");
        snprintf(target_files[0].filepath, sizeof(target_files[0].filepath),
                 "%s/%.*s", save_path, (int)name_node->string.length, name_node->string.data);
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

        for (int i = 0; i < (int)*num_files; i++) {
            const BencodeNode *file_dict   = files_list->list.items[i];
            const BencodeNode *length_node = getDictValue(file_dict, "length");
            const BencodeNode *path_list   = getDictValue(file_dict, "path");

            if (!path_list || path_list->type != BEN_LIST ||
                path_list->list.length == 0 || !length_node) {
                target_files[i].filepath[0]  = '\0';
                target_files[i].length       = 0;
                target_files[i].global_start = current_global_offset;
                target_files[i].global_end   = current_global_offset;
                continue;
            }

            // save path
            snprintf(target_files[i].filepath, sizeof(target_files[i].filepath), "%s", save_path);
            for (size_t seg = 0; seg < path_list->list.length; seg++) {
                const BencodeNode *s = path_list->list.items[seg];
                if (!s || s->type != BEN_STR) continue;

                const size_t current_len = strlen(target_files[i].filepath);
                snprintf(target_files[i].filepath + current_len,
                         sizeof(target_files[i].filepath) - current_len,
                         "/%.*s", (int)s->string.length, s->string.data);
            }

            target_files[i].length       = length_node->intValue;
            target_files[i].global_start = current_global_offset;
            target_files[i].global_end   = current_global_offset + target_files[i].length;
            current_global_offset        = target_files[i].global_end;
        }
        return target_files;
    }

    fprintf(stderr, "Invalid files dictionary in torrent.\n");
    return NULL;
}

bool read_piece_from_disk(const uint32_t piece_index, const size_t piece_length, unsigned char *out_buffer,
                          const EndFile *end_files, const int num_files) {
    const size_t piece_global_start = piece_index * piece_length;
    const size_t piece_global_end = piece_global_start + piece_length;
    size_t bytes_read = 0;

    for (int i = 0; i < num_files; i++) {
        const EndFile *file = &end_files[i];

        if (piece_global_end <= file->global_start || piece_global_start >= file->global_end) continue;

        const size_t overlap_end = piece_global_end < file->global_end ? piece_global_end : file->global_end;
        const size_t overlap_start = piece_global_start > file->global_start ? piece_global_start : file->global_start;
        const size_t read_length = overlap_end - overlap_start;
        const size_t local_file_offset = overlap_start - file->global_start;

        FILE *f = fopen(file->filepath, "rb");
        if (!f) return false;

        fseek(f, local_file_offset, SEEK_SET);
        const size_t actual_read = fread(out_buffer + bytes_read, 1, read_length, f);
        fclose(f);

        if (actual_read != read_length) return false;
        bytes_read += read_length;
        if (bytes_read >= piece_length) break;
    }
    return bytes_read > 0;
}