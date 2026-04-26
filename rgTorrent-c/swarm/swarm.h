#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "file_saver.h"

typedef struct TorrentEntry TorrentEntry;

typedef enum {
    PEER_STATE_CONNECTING = 0,
    PEER_STATE_HANDSHAKING,
    PEER_STATE_WAITING_BITFIELD,
    PEER_STATE_WAITING_UNCHOKE,
    PEER_STATE_DOWNLOADING,
    PEER_STATE_DEAD
} PeerConnectionState;

typedef struct {
    int sockfd;
    PeerConnectionState state;
    bool *inventory;
    int current_piece_assigned;
    uint32_t current_block_offset;
    unsigned char *piece_buffer;
} PeerConnection;

typedef enum {
    PIECE_MISSING = 0,
    PIECE_PENDING = 1,
    PIECE_DONE = 2
} PieceState;

void start_swarm(TorrentEntry *e, const unsigned char *peers_list, size_t peers_count, const unsigned char *pieces_hashes, const EndFile *end_files, int num_files);