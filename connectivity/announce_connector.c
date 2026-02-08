#include "announce_connector.h"
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <uriparser/Uri.h>
#include <unistd.h>
#include <endian.h>

#define PROTOCOL_ID 0x41727101980LL

void udp_send_announce_request(int sockfd, const struct addrinfo *server_info, int64_t connection_id,
                               const UdpAnnounceRequest *announce);

char *get_peers_list(const UdpAnnounceRequest *announce) {
    UriUriA announce_uri;
    const char *errorPos;
    if (uriParseSingleUriA(&announce_uri, announce->announce_address, &errorPos) != URI_SUCCESS) {
        fprintf(stderr, "Invalid URI at: %s\n", errorPos);
        return NULL;
    }

    struct addrinfo hints = {0}, *server_info;
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    char tracker_host[256], tracker_port[10];

    const int host_len = announce_uri.hostText.afterLast - announce_uri.hostText.first;
    const int port_len = announce_uri.portText.afterLast - announce_uri.portText.first;

    if (host_len > 0 && host_len < sizeof(tracker_host)) {
        strncpy(tracker_host, announce_uri.hostText.first, host_len);
        tracker_host[host_len] = '\0';
    }

    if (port_len > 0 && port_len < sizeof(tracker_port)) {
        strncpy(tracker_port, announce_uri.portText.first, port_len);
        tracker_port[port_len] = '\0';
    }

    // TODO: temporarily, as the tested ones use HTTP with TCP instead of UDP
    strncpy(tracker_host, "tracker.opentrackr.org", sizeof(tracker_host));
    strncpy(tracker_port, "1337", sizeof(tracker_port));

    const int status = getaddrinfo(tracker_host, tracker_port, &hints, &server_info);
    if (status != 0) {
        fprintf(stderr, "DNS Lookup failed: %s\n", gai_strerror(status));
        return NULL;
    }

    const int sockfd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (sockfd < 0) {
        perror("Socket creation failed");
        freeaddrinfo(server_info);
        return NULL;
    }

    // TODO: timeout retrial https://www.bittorrent.org/beps/bep_0015.html
    const struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    UdpConnectRequestPacket connect_request;
    connect_request.protocol_id = htobe64(PROTOCOL_ID);
    connect_request.action = htobe32(ConnectRequest);

    // TODO: true random?
    srand(time(NULL));
    const int32_t transaction_id = rand();
    connect_request.transaction_id = htobe32(transaction_id);

    if (sendto(sockfd, &connect_request, sizeof(connect_request), 0,
               server_info->ai_addr, server_info->ai_addrlen) < 0) {
        perror("Sendto failed");
        close(sockfd);
        freeaddrinfo(server_info);
        return NULL;
    }

    UdpConnectResponsePacket connect_response;
    socklen_t addr_len = server_info->ai_addrlen;

    const ssize_t received = recvfrom(sockfd, &connect_response, sizeof(connect_response), 0,
                                      server_info->ai_addr, &addr_len);

    if (received < (ssize_t) sizeof(connect_response)) {
        perror("Receive failed or packet too small");
        close(sockfd);
        freeaddrinfo(server_info);
        return NULL;
    }

    if (be32toh(connect_response.transaction_id) != transaction_id) {
        fprintf(stderr, "Transaction ID mismatch: Expected %d, received %d\n", transaction_id,
                be32toh(connect_response.transaction_id));
        return NULL;
    }

    if (be32toh(connect_response.action) != ConnectRequest) {
        fprintf(stderr, "Tracker Error: Action is not %d (Connect)\n", ConnectRequest);
        return NULL;
    }

    const int64_t connection_id = be64toh(connect_response.connection_id);
    printf("Handshake succeeded. Connection ID: %ld\n", connection_id);

    udp_send_announce_request(sockfd, server_info, connection_id, announce);

    close(sockfd);
    freeaddrinfo(server_info);
    return NULL;
}

void udp_send_announce_request(const int sockfd, const struct addrinfo *server_info, const int64_t connection_id,
                               const UdpAnnounceRequest *announce) {
    UdpAnnounceRequestPacket announce_req;
    announce_req.connection_id = htobe64(connection_id);
    announce_req.action = htobe32(1); // TODO: 1 = Announce, use enum

    const int32_t transaction_id = rand();
    announce_req.transaction_id = htobe32(transaction_id);

    memcpy(announce_req.info_hash, announce->info_hash, 20);
    memcpy(announce_req.peer_id, announce->peer_id, 20);

    announce_req.downloaded = htobe64(0);
    announce_req.left = htobe64(announce->left);
    announce_req.uploaded = htobe64(0);
    announce_req.event = htobe32(0); // TODO: 0 = None, use enum
    announce_req.ip_address = htobe32(0);
    announce_req.key = htobe32(rand());
    announce_req.num_want = htobe32(-1); // TODO: -1 = Default, use enum
    announce_req.port = htobe16(announce->port);

    if (sendto(sockfd, &announce_req, sizeof(announce_req), 0, server_info->ai_addr, server_info->ai_addrlen) < 0) {
        perror("Announce send failed");
        return;
    }

    unsigned char response_buffer[2048];
    socklen_t addr_len = server_info->ai_addrlen;

    if (recvfrom(sockfd, response_buffer, sizeof(response_buffer), 0,
                                      server_info->ai_addr, &addr_len) < 20) {
        perror("Announce receive failed or too small.");
        return;
    }

    const UdpAnnounceResponsePacket *announce_resp = (UdpAnnounceResponsePacket*) response_buffer;

    if (be32toh(announce_resp->transaction_id) != transaction_id) {
        fprintf(stderr, "Announce Transaction ID mismatch\n");
        return;
    }

    const int leechers = be32toh(announce_resp->leechers);
    const int seeders = be32toh(announce_resp->seeders);
    printf("Tracker Reply: %d seeders, %d leechers\n", seeders, leechers);

    const unsigned char *peers = response_buffer + 20;
    const int total_peers = leechers + seeders;

    for (int i = 0; i < total_peers; i++) {
        const unsigned char *p = peers + i * 6;
        printf("Peer %d: %d.%d.%d.%d:%d\n",
               i + 1,
               p[0], p[1], p[2], p[3],
               p[4] << 8 | p[5]);
    }
}
