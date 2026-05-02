#include "torrent_creator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>
#include <openssl/sha.h>

typedef struct {
    char absolute_path[1024];
    char relative_path[1024];
    size_t size;
} FileMeta;

static int compare_files(const void *a, const void *b) {
    return strcmp(((FileMeta*)a)->relative_path, ((FileMeta*)b)->relative_path);
}

static void scan_directory(const char *base_path, const char *current_path, FileMeta **files, int *count, int *cap) {
    DIR *dir = opendir(current_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                scan_directory(base_path, full_path, files, count, cap);
            } else if (S_ISREG(st.st_mode)) {
                if (*count >= *cap) {
                    *cap *= 2;
                    *files = realloc(*files, *cap * sizeof(FileMeta));
                }
                strncpy((*files)[*count].absolute_path, full_path, 1024);

                const char *rel = full_path + strlen(base_path);
                if (rel[0] == '/') rel++;
                strncpy((*files)[*count].relative_path, rel, 1024);

                (*files)[*count].size = st.st_size;
                (*count)++;
            }
        }
    }
    closedir(dir);
}

int ts_create_torrent(const char *source_dir, const char *output_path, const char *tracker_url, int piece_length) {
    printf("[INFO] Starting torrent creation for: %s\n", source_dir);

    int capacity = 10;
    int file_count = 0;
    FileMeta *files = malloc(capacity * sizeof(FileMeta));
    scan_directory(source_dir, source_dir, &files, &file_count, &capacity);

    if (file_count == 0) {
        fprintf(stderr, "[INFO] Error: No files found in directory.\n");
        free(files);
        return -1;
    }
    qsort(files, file_count, sizeof(FileMeta), compare_files);

    const char *base_name = strrchr(source_dir, '/');
    base_name = base_name ? base_name + 1 : source_dir;

    uint64_t total_size = 0;
    for (int i = 0; i < file_count; i++) total_size += files[i].size;

    size_t num_pieces = (total_size + piece_length - 1) / piece_length;
    unsigned char *hashes = malloc(num_pieces * SHA_DIGEST_LENGTH);

    unsigned char *piece_buffer = malloc(piece_length);
    size_t buffer_offset = 0;
    size_t current_piece = 0;

    printf("[Creator] Hashing %ld bytes into %zu pieces...\n", total_size, num_pieces);

    for (int i = 0; i < file_count; i++) {
        FILE *f = fopen(files[i].absolute_path, "rb");
        if (!f) continue;

        while (true) {
            size_t bytes_to_read = piece_length - buffer_offset;
            size_t read_bytes = fread(piece_buffer + buffer_offset, 1, bytes_to_read, f);
            buffer_offset += read_bytes;

            if (buffer_offset == (size_t)piece_length) {
                SHA1(piece_buffer, piece_length, hashes + current_piece * SHA_DIGEST_LENGTH);
                current_piece++;
                buffer_offset = 0;
            }
            if (read_bytes < bytes_to_read) break;
        }
        fclose(f);
    }

    if (buffer_offset > 0) {
        SHA1(piece_buffer, buffer_offset, hashes + (current_piece * SHA_DIGEST_LENGTH));
    }

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        free(files); free(hashes); free(piece_buffer);
        return -1;
    }

    fprintf(out, "d8:announce%zu:%s13:creation datei%lde4:infod5:filesl",
            strlen(tracker_url), tracker_url, time(NULL));

    for (int i = 0; i < file_count; i++) {
        fprintf(out, "d6:lengthi%zue4:pathl", files[i].size);
        char *path_copy = strdup(files[i].relative_path);
        char *token = strtok(path_copy, "/");
        while (token) {
            fprintf(out, "%zu:%s", strlen(token), token);
            token = strtok(NULL, "/");
        }
        free(path_copy);
        fprintf(out, "ee");
    }

    fprintf(out, "e4:name%zu:%s12:piece lengthi%de6:pieces%zu:",
            strlen(base_name), base_name, piece_length, num_pieces * SHA_DIGEST_LENGTH);

    fwrite(hashes, 1, num_pieces * SHA_DIGEST_LENGTH, out);
    fprintf(out, "ee");
    fclose(out);

    free(piece_buffer);
    free(hashes);
    free(files);
    return 0;
}