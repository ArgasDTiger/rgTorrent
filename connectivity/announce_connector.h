#ifndef ANNOUNCE_CONNECTOR_H
#define ANNOUNCE_CONNECTOR_H
#define DEFAULT_ANNOUNCE_PORT 6881;
#include <stdint.h>

typedef struct {
    unsigned char* announce_address;
    unsigned char* info_hash;
    unsigned char* peer_id;
    unsigned char* ip;
    int port;
    long uploaded;
    long downloaded;
    long left;
} UdpAnnounceRequest;

// https://www.bittorrent.org/beps/bep_0015.html
typedef struct  {
    int64_t protocol_id;
    int32_t action;
    int32_t transaction_id;
} __attribute__((packed)) UdpConnectRequestPacket;

typedef struct {
    int32_t action;
    int32_t transaction_id;
    int64_t connection_id;
} __attribute__((packed)) UdpConnectResponsePacket;

typedef struct {
    int64_t connection_id;
    int32_t action;
    int32_t transaction_id;
    unsigned char info_hash[20];
    unsigned char peer_id[20];
    int64_t downloaded;
    int64_t left;
    int64_t uploaded;
    int32_t event;
    int32_t ip_address;
    int32_t key;
    int32_t num_want;
    uint16_t port;
} __attribute__((packed)) UdpAnnounceRequestPacket;

typedef struct {
    int32_t action;
    int32_t transaction_id;
    int32_t interval;
    int32_t leechers;
    int32_t seeders;
} __attribute__((packed)) UdpAnnounceResponsePacket;

enum NormalAnnounce{
    ConnectRequest = 0,
    ConnectResponse = 1,
    AnnounceRequest = 2,
    AnnounceResponse = 3,
};

char* get_peers_list(const UdpAnnounceRequest* announce);
#endif // ANNOUNCE_CONNECTOR_H
