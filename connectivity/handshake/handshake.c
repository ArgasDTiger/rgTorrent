#include <stddef.h>
#include "handshake.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BITTORRENT_PROTOCOL_STR "BitTorrent protocol"

bool handshake_by_address(const char* ip, int port, const PeerHandshake* handshake_to_peer);

void establish_handshake(const unsigned char* peers_list, const size_t peers_count, const uint8_t *info_hash, const uint8_t *peer_id) {
    if (peers_count <= 0) return;

    PeerHandshake my_handshake;
    my_handshake.pstrlen = 19;
    memcpy(my_handshake.pstr, BITTORRENT_PROTOCOL_STR, 19);
    memset(my_handshake.reserved, 0, 8);
    memcpy(my_handshake.info_hash, info_hash, 20);
    memcpy(my_handshake.peer_id, peer_id, 20);

    for (int i = 0; i < peers_count; i++) {
        const unsigned char *p = peers_list + i * 6;
        const int port = p[4] << 8 | p[5];

        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);

        const bool is_success = handshake_by_address(ip_str, port, &my_handshake);
        if (is_success) {
            break;
        }
    }
}

bool handshake_by_address(const char* ip, const int port, const PeerHandshake *handshake_to_peer) {
    printf("Trying to establish a connection to %s:%d...\n", ip, port);
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed.");
        return false;
    }

    const struct timeval tv = {.tv_sec = 3, .tv_usec = 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in peer_addr = {0};
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &peer_addr.sin_addr) <= 0) {
        perror("Invalid IP address.");
        close(sockfd);
        return false;
    }

    if (connect(sockfd, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) < 0) {
        printf("Connection failed or timed out.\n");
        close(sockfd);
        return false;
    }

    printf("Connected! Sending handshake...\n");

    if (send(sockfd, handshake_to_peer, sizeof(PeerHandshake), 0) != sizeof(PeerHandshake)) {
        printf("Failed to send full handshake.\n");
        close(sockfd);
        return false;
    }

    PeerHandshake handshake_from_peer;
    ssize_t received = recv(sockfd, &handshake_from_peer, sizeof(handshake_from_peer), MSG_WAITALL);

    if (received < 68) {
        printf("Peer dropped connection or sent invalid handshake (got %ld bytes).\n", received);
        close(sockfd);
        return false;
    }

    if (memcmp(handshake_to_peer->info_hash, handshake_from_peer.info_hash, 20) != 0) {
        printf("Info hash mismatch occurred, different file was sent.\n");
        close(sockfd);
        return false;
    }

    printf("Valid handshake received from %s:%d.\n", ip, port);

    uint32_t message_length_net;
    received = recv(sockfd, &message_length_net, sizeof(message_length_net), MSG_WAITALL);
    if (received <= 0) {
        printf("Peer dropped connection or sent invalid message length after a handshake.\n");
        close(sockfd);
        return false;
    }

    const uint32_t message_length = be32toh(message_length_net);
    if (message_length <= 0) {
        printf("Received Keep-Alive message.\n");
        close(sockfd);
        return false;
    }

    uint8_t msg_id;
    received = recv(sockfd, &msg_id, 1, MSG_WAITALL);
    if (received <= 0) {
        printf("Failed to read message ID.\n");
        close(sockfd);
        return false;
    }

    printf("Received Message ID: %d (Length: %u bytes)\n", msg_id, message_length);

    const uint32_t payload_length = message_length - 1;
    if (payload_length > 0) {
        unsigned char *payload = malloc(payload_length);
        received = recv(sockfd, payload, payload_length, MSG_WAITALL);

        if (msg_id == 5) {
            printf("Received a BITFIELD message.\n");
        }

        free(payload);
    }

    close(sockfd);
    return true;
}

