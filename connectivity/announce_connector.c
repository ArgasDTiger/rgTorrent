#include "announce_connector.h"
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <uriparser/Uri.h>
#include <unistd.h>
#include <endian.h>
#include "request_helpers.h"
#include "bencode_parser.h"
#include "bencoder.h"

#define PROTOCOL_ID 0x41727101980LL

char* udp_send_announce_request(int sockfd, const struct addrinfo *server_info, int64_t connection_id,
                               const UdpAnnounceRequest *announce, size_t *out_len);
char* udp_get_peers_list(char* tracker_host, char* tracker_port, const UdpAnnounceRequest *announce, size_t *out_len);
char* http_get_peers_list(char* tracker_host, const char* tracker_port, const UdpAnnounceRequest *announce, size_t *out_len);
char* parse_peers_from_http_body(char* body, size_t body_length, size_t *out_peers_length);

char *get_peers_list(const UdpAnnounceRequest *announce, size_t *out_len) {
    UriUriA announce_uri;
    const char *errorPos;
    if (uriParseSingleUriA(&announce_uri, announce->announce_address, &errorPos) != URI_SUCCESS) {
        fprintf(stderr, "Invalid URI at: %s\n", errorPos);
        return NULL;
    }

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

    const long scheme_len = announce_uri.scheme.afterLast - announce_uri.scheme.first;
    if (scheme_len == 3 && strncmp(announce_uri.scheme.first, "udp", scheme_len) == 0) {
        char* udp_result = udp_get_peers_list(tracker_host, tracker_port, announce, out_len);
        return udp_result;
    }
    if ((scheme_len == 4 && strncmp(announce_uri.scheme.first, "http", scheme_len) == 0) || (scheme_len == 5 && strncmp(announce_uri.scheme.first, "https", scheme_len) == 0)) {
        char* http_result = http_get_peers_list(tracker_host, tracker_port, announce, out_len);
        return http_result;
    }
    fprintf(stderr, "Failed to resolve scheme for announce address, found: %s\n", announce_uri.scheme.first);
    return NULL;
}

char* http_get_peers_list(char* tracker_host, const char* tracker_port, const UdpAnnounceRequest *announce, size_t* out_len) {
    struct addrinfo hints = {0}, *server_info;
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    printf("Connecting to HTTP Tracker: %s:%s\n", tracker_host, tracker_port);

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

    if (connect(sockfd, server_info->ai_addr, server_info->ai_addrlen) < 0) {
        perror("Connect failed");
        close(sockfd);
        freeaddrinfo(server_info);
        return NULL;
    }

    freeaddrinfo(server_info);

    char encoded_hash[61];
    char encoded_peer_id[61];
    url_encode(announce->info_hash, 20, encoded_hash);
    url_encode(announce->peer_id, 20, encoded_peer_id);

    char request[2048];
    snprintf(request, sizeof(request),
        "GET /announce?info_hash=%s&peer_id=%s&port=%d&uploaded=0&downloaded=0&left=%ld&compact=1&event=started HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        encoded_hash, encoded_peer_id, announce->port, announce->left, tracker_host
    );

    if (send(sockfd, request, strlen(request), 0) < 0) {
        perror("Send failed");
        close(sockfd);
        return NULL;
    }

    // TODO: dont predefine buffer size, define size based on recv
    char response_buf[16384];
    ssize_t total_received = 0;
    ssize_t received;

    while ((received = recv(sockfd, response_buf + total_received, sizeof(response_buf) - 1 - total_received, 0)) > 0) {
        total_received += received;
        // TODO: remove when not predefining size
        if (total_received >= sizeof(response_buf) - 1) {
            break;
        }
    }

    if (total_received < 0) {
        perror("Recv failed");
        close(sockfd);
        return NULL;
    }

    response_buf[total_received] = '\0';
    close(sockfd);

    // HTTP separates headers and body with "\r\n\r\n"
    char *body = strstr(response_buf, "\r\n\r\n");
    if (!body) {
        body = strstr(response_buf, "\n\n");
        if (!body) {
            fprintf(stderr, "Invalid HTTP response\n");
            return NULL;
        }
        body += 2;
    } else {
        body += 4;
    }

    const long body_offset = body - response_buf;
    const size_t body_length = total_received - body_offset;

    char *peers = parse_peers_from_http_body(body, body_length, out_len);
    return peers;
}

char* parse_peers_from_http_body(char* body, const size_t body_length, size_t *out_peers_length) {
    FILE *mem_file = fmemopen(body, body_length, "rb");
    if (!mem_file) {
        perror("fmemopen failed");
        return NULL;
    }

    BencodeContext ctx;
    ctx.file = mem_file;
    ctx.hasError = false;
    ctx.errorPosition = 0;
    memset(ctx.errorMsg, 0, sizeof(ctx.errorMsg));

    BencodeNode *root = parseDict(&ctx);

    if (ctx.hasError || !root) {
        if (ctx.hasError) fprintf(stderr, "Parser Error: %s\n", ctx.errorMsg);
        if (root) freeBencodeNode(root);
        fclose(mem_file);
        return NULL;
    }

    BencodeNode* peers_node = getDictValue(root, "peers");

    if (!peers_node || peers_node->type != BEN_STR) {
        fprintf(stderr, "No valid 'peers' key found in response.\n");
        freeBencodeNode(root);
        fclose(mem_file);
        return NULL;
    }

    const size_t data_len = peers_node->string.length;
    char *peers_copy = malloc(data_len);

    if (peers_copy) {
        memcpy(peers_copy, peers_node->string.data, data_len);
        if (out_peers_length) {
            *out_peers_length = data_len;
        }
    }

    freeBencodeNode(root);
    fclose(mem_file);
    return peers_copy;
}

char* udp_get_peers_list(char* tracker_host, char* tracker_port, const UdpAnnounceRequest *announce, size_t *out_len) {
    struct addrinfo hints = {0}, *server_info;
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

     // TODO: temporarily, as the tested ones use HTTP with TCP instead of UDP
    strncpy(tracker_host, "tracker.opentrackr.org", sizeof(&tracker_host));
    strncpy(tracker_port, "1337", sizeof(&tracker_port));

    const int status = getaddrinfo(tracker_host, tracker_port, &hints, &server_info);
    if (status != 0) {
        fprintf(stderr, "DNS Lookup failed: %s\n", gai_strerror(status));
        freeaddrinfo(server_info);
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

    char* peers = udp_send_announce_request(sockfd, server_info, connection_id, announce, out_len);

    close(sockfd);
    freeaddrinfo(server_info);
    return peers;
}

char* udp_send_announce_request(const int sockfd, const struct addrinfo *server_info, const int64_t connection_id,
                               const UdpAnnounceRequest *announce, size_t *out_len) {
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
        return NULL;
    }

    unsigned char response_buffer[2048];
    socklen_t addr_len = server_info->ai_addrlen;

    const ssize_t resp_len = recvfrom(sockfd, response_buffer, sizeof(response_buffer), 0,
                                      server_info->ai_addr, &addr_len);

    if (resp_len < 20) {
        perror("Announce receive failed or too small.");
        return NULL;
    }

    const UdpAnnounceResponsePacket *announce_resp = (UdpAnnounceResponsePacket*) response_buffer;

    if (be32toh(announce_resp->transaction_id) != transaction_id) {
        fprintf(stderr, "Announce Transaction ID mismatch\n");
        return NULL;
    }

    const size_t peers_size = resp_len - 20;
    if (out_len) {
        *out_len = peers_size;
    }
    char *peers = malloc(peers_size);
    if (!peers) return NULL;

    memcpy(peers, response_buffer + 20, peers_size);

    return peers;
}
