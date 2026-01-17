#ifndef ANNOUNCE_CONNECTOR_H
#define ANNOUNCE_CONNECTOR_H
#define DEFAULT_ANNOUNCE_PORT 6881;

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

char* get_peers_list(const UdpAnnounceRequest* announce);
#endif // ANNOUNCE_CONNECTOR_H
