#ifndef DOWNLOADER_H
#define DOWNLOADER_H
#include <stdbool.h>

bool download_piece(int sockfd, int piece_index);
#endif // DOWNLOADER_H
