#ifndef HANDSHAKE_H
#define HANDSHAKE_H
#include <stdint.h>

typedef struct  {
    uint8_t pstrlen;       // 19
    char pstr[19];         // "BitTorrent protocol"
    uint8_t reserved[8];
    uint8_t info_hash[20];
    uint8_t peer_id[20];
} __attribute__((packed)) PeerHandshake;

void establish_handshake(const unsigned char* peers_list, size_t peers_count, const uint8_t *info_hash, const uint8_t *peer_id);
#endif // HANDSHAKE_H
