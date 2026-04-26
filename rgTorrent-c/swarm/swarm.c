#include "swarm.h"
#include "torrent_session.h"
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "handshake.h"
#include "file_saver.h"
#include <openssl/sha.h>

#define MAX_PEERS 30
#define BITTORENT_PROTOCOL "BitTorrent protocol"

static void set_nonblocking(const int sockfd) {
    const int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

void request_block(const int sockfd, const uint32_t piece_index, const uint32_t block_offset,
                   const uint32_t block_length) {
    unsigned char req_msg[17];
    const uint32_t net_len = htonl(13);
    const uint8_t msg_id = 6;
    const uint32_t net_index = htonl(piece_index);
    const uint32_t net_begin = htonl(block_offset);
    const uint32_t net_length = htonl(block_length);

    memcpy(req_msg, &net_len, 4);
    req_msg[4] = msg_id;
    memcpy(req_msg + 5, &net_index, 4);
    memcpy(req_msg + 9, &net_begin, 4);
    memcpy(req_msg + 13, &net_length, 4);
    send(sockfd, req_msg, 17, 0);
}

static bool read_exactly(const int fd, void *buf, const size_t count) {
    size_t bytes_read = 0;
    while (bytes_read < count) {
        const ssize_t res = recv(fd, (char *) buf + bytes_read, count - bytes_read, 0);
        if (res > 0) {
            bytes_read += res;
        } else if (res == 0) {
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // let's forgive slow peers
                struct pollfd pfd = {.fd = fd, .events = POLLIN};
                if (poll(&pfd, 1, 15000) <= 0) return false;
            } else {
                return false;
            }
        }
    }
    return true;
}

int get_next_piece_to_download(TorrentEntry *e, const bool *peer_inventory) {
    int selected_piece = -1;
    pthread_mutex_lock(&e->lock);
    for (size_t i = 0; i < e->total_pieces; i++) {
        if (e->piece_states[i] == PIECE_MISSING && peer_inventory[i] == true) {
            e->piece_states[i] = PIECE_PENDING;
            selected_piece = (int) i;
            break;
        }
    }
    pthread_mutex_unlock(&e->lock);
    return selected_piece;
}

void start_swarm(TorrentEntry *e, const unsigned char *peers_list, const size_t peers_count,
                 const unsigned char *pieces_hashes, const EndFile *end_files, const int num_files) {
    struct pollfd poll_fds[MAX_PEERS];
    PeerConnection peers[MAX_PEERS];

    for (int i = 0; i < MAX_PEERS; i++) {
        poll_fds[i].fd = -1;
        peers[i].state = PEER_STATE_DEAD;
        peers[i].inventory = NULL;
    }

    const int spawn_count = peers_count < MAX_PEERS ? peers_count : MAX_PEERS;

    for (int i = 0; i < spawn_count; i++) {
        const unsigned char *p = peers_list + i * 6;
        const int port = p[4] << 8 | p[5];
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);

        const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) continue;

        set_nonblocking(sockfd);

        struct sockaddr_in peer_addr = {0};
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip_str, &peer_addr.sin_addr);

        connect(sockfd, (struct sockaddr *) &peer_addr, sizeof(peer_addr));

        poll_fds[i].fd = sockfd;
        poll_fds[i].events = POLLIN | POLLOUT;
        peers[i].sockfd = sockfd;
        peers[i].state = PEER_STATE_CONNECTING;
    }

    PeerHandshake established_handshake;
    established_handshake.pstrlen = 19;
    memcpy(established_handshake.pstr, BITTORENT_PROTOCOL, 19);
    memset(established_handshake.reserved, 0, 8);
    memcpy(established_handshake.info_hash, e->info_hash, 20);
    memcpy(established_handshake.peer_id, e->peer_id, 20);

    while (true) {
        const int activity = poll(poll_fds, MAX_PEERS, 1000);
        if (activity < 0) break;
        if (activity == 0) continue;

        for (int i = 0; i < MAX_PEERS; i++) {
            if (poll_fds[i].fd == -1) continue;

            if (poll_fds[i].revents & POLLIN) {
                if (peers[i].state == PEER_STATE_HANDSHAKING) {
                    PeerHandshake peer_reply;
                    const ssize_t received = recv(poll_fds[i].fd, &peer_reply, sizeof(PeerHandshake), 0);

                    if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;

                    if (received != sizeof(PeerHandshake) || memcmp(peer_reply.info_hash, e->info_hash, 20) != 0) {
                        close(poll_fds[i].fd);
                        poll_fds[i].fd = -1;
                        peers[i].state = PEER_STATE_DEAD;
                        continue;
                    }
                    peers[i].state = PEER_STATE_WAITING_BITFIELD;
                } else if (peers[i].state == PEER_STATE_WAITING_BITFIELD) {
                    uint32_t msg_len_net = 0;
                    const ssize_t recvd = recv(poll_fds[i].fd, &msg_len_net, 4, 0);

                    if (recvd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                        goto drop_peer_early;
                    }
                    if (recvd == 0) {
                        goto drop_peer_early;
                    }
                    if (recvd < 4) {
                        if (!read_exactly(poll_fds[i].fd, ((char *) &msg_len_net) + recvd, 4 - recvd))
                            goto drop_peer_early;
                    }

                    const uint32_t msg_len = ntohl(msg_len_net);
                    peers[i].inventory = calloc(e->total_pieces, sizeof(bool));

                    if (msg_len > 0) {
                        uint8_t msg_id;
                        if (!read_exactly(poll_fds[i].fd, &msg_id, 1)) goto drop_peer_early;

                        const uint32_t payload_len = msg_len - 1;
                        if (msg_id == 5 && payload_len > 0) {
                            unsigned char *payload = malloc(payload_len);
                            if (read_exactly(poll_fds[i].fd, payload, payload_len)) {
                                for (size_t p = 0; p < e->total_pieces; p++) {
                                    const size_t byte_index = p / 8;
                                    const size_t bit_index = 7 - (p % 8);
                                    if (byte_index < payload_len && payload[byte_index] >> bit_index & 1) {
                                        peers[i].inventory[p] = true;
                                    }
                                }
                            }
                            free(payload);
                        } else if (payload_len > 0) {
                            unsigned char *temp = malloc(payload_len);
                            read_exactly(poll_fds[i].fd, temp, payload_len);
                            free(temp);
                        }
                    }

                    const uint8_t interested_msg[5] = {0, 0, 0, 1, 2};
                    send(poll_fds[i].fd, interested_msg, 5, 0);
                    peers[i].state = PEER_STATE_WAITING_UNCHOKE;
                    continue;

                // it's not like using goto is good... but yeah, here we are
                drop_peer_early:
                    close(poll_fds[i].fd);
                    poll_fds[i].fd = -1;
                    peers[i].state = PEER_STATE_DEAD;
                } else if (peers[i].state == PEER_STATE_WAITING_UNCHOKE) {
                    uint32_t msg_len_net = 0;
                    const ssize_t recvd = recv(poll_fds[i].fd, &msg_len_net, 4, 0);

                    if (recvd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                        goto drop_peer_early;
                    }
                    if (recvd == 0) {
                        goto drop_peer_early;
                    }
                    if (recvd < 4) {
                        if (!read_exactly(poll_fds[i].fd, ((char *) &msg_len_net) + recvd, 4 - recvd))
                            goto drop_peer_early;
                    }

                    const uint32_t msg_len = ntohl(msg_len_net);
                    if (msg_len == 0) continue;

                    uint8_t msg_id;
                    if (!read_exactly(poll_fds[i].fd, &msg_id, 1)) goto drop_peer_early;

                    const uint32_t payload_len = msg_len - 1;
                    if (payload_len > 0) {
                        unsigned char *temp = malloc(payload_len);
                        read_exactly(poll_fds[i].fd, temp, payload_len);
                        free(temp);
                    }

                    if (msg_id == 1) {
                        const int next_piece = get_next_piece_to_download(e, peers[i].inventory);
                        if (next_piece == -1) goto drop_peer_early;

                        peers[i].current_piece_assigned = next_piece;
                        peers[i].current_block_offset = 0;
                        peers[i].piece_buffer = malloc(e->piece_length);

                        uint32_t block_size = DEFAULT_BLOCK_SIZE;
                        size_t current_piece_size = e->piece_length;
                        if (next_piece == (int) e->total_pieces - 1) {
                            const size_t remainder = e->size_bytes % e->piece_length;
                            if (remainder != 0) current_piece_size = remainder;
                        }
                        if (peers[i].current_block_offset + block_size > current_piece_size) {
                            block_size = current_piece_size - peers[i].current_block_offset;
                        }

                        request_block(poll_fds[i].fd, next_piece, 0, block_size);
                        peers[i].state = PEER_STATE_DOWNLOADING;
                    }
                } else if (peers[i].state == PEER_STATE_DOWNLOADING) {
                    uint32_t msg_len_net;
                    const ssize_t res = recv(poll_fds[i].fd, &msg_len_net, 4, 0);

                    if (res < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                        goto drop_peer;
                    }
                    if (res == 0) {
                        goto drop_peer;
                    }
                    if (res < 4) {
                        if (!read_exactly(poll_fds[i].fd, (char *) &msg_len_net + res, 4 - res)) goto drop_peer;
                    }

                    const uint32_t msg_len = ntohl(msg_len_net);
                    if (msg_len == 0) continue;

                    uint8_t msg_id;
                    if (!read_exactly(poll_fds[i].fd, &msg_id, 1)) goto drop_peer;

                    const uint32_t payload_len = msg_len - 1;
                    if (msg_id != 7) {
                        if (payload_len > 0) {
                            unsigned char *temp = malloc(payload_len);
                            read_exactly(poll_fds[i].fd, temp, payload_len);
                            free(temp);
                        }
                        continue;
                    }

                    uint32_t net_index, net_begin;
                    if (!read_exactly(poll_fds[i].fd, &net_index, 4)) goto drop_peer;
                    if (!read_exactly(poll_fds[i].fd, &net_begin, 4)) goto drop_peer;

                    const uint32_t block_index = ntohl(net_index);
                    const uint32_t block_begin = ntohl(net_begin);
                    const uint32_t block_data_len = payload_len - 8;

                    if (block_index != peers[i].current_piece_assigned) goto drop_peer;

                    unsigned char *block_data = malloc(block_data_len);
                    if (!read_exactly(poll_fds[i].fd, block_data, block_data_len)) {
                        free(block_data);
                        goto drop_peer;
                    }

                    memcpy(peers[i].piece_buffer + block_begin, block_data, block_data_len);
                    free(block_data);

                    peers[i].current_block_offset += block_data_len;

                    size_t current_piece_size = e->piece_length;
                    if (block_index == (int) e->total_pieces - 1) {
                        const size_t remainder = e->size_bytes % e->piece_length;
                        if (remainder != 0) current_piece_size = remainder;
                    }

                    // check did we finish the piece
                    if (peers[i].current_block_offset >= current_piece_size) {
                        unsigned char hash[SHA_DIGEST_LENGTH];
                        SHA1(peers[i].piece_buffer, current_piece_size, hash);
                        const unsigned char *expected_hash = pieces_hashes + (block_index * SHA_DIGEST_LENGTH);

                        if (memcmp(hash, expected_hash, SHA_DIGEST_LENGTH) != 0) {
                            printf("[Swarm] Slot %d Piece %d failed hash verification!\n", i, block_index);
                            goto drop_peer;
                        }

                        printf("[Swarm] Slot %d finished Piece %d! Verified and saved.\n", i, block_index);
                        write_piece_to_disk(block_index, e->piece_length, peers[i].piece_buffer, end_files, num_files);

                        pthread_mutex_lock(&e->lock);
                        e->piece_states[block_index] = PIECE_DONE;
                        e->pieces_completed++;
                        e->progress = (double) e->pieces_completed / (double) e->total_pieces;
                        pthread_mutex_unlock(&e->lock);

                        free(peers[i].piece_buffer);
                        peers[i].piece_buffer = NULL;

                        const int next_piece = get_next_piece_to_download(e, peers[i].inventory);
                        if (next_piece != -1) {
                            peers[i].current_piece_assigned = next_piece;
                            peers[i].current_block_offset = 0;
                            peers[i].piece_buffer = malloc(e->piece_length);

                            uint32_t next_block_size = DEFAULT_BLOCK_SIZE;
                            size_t next_piece_size = e->piece_length;
                            if (next_piece == (int) e->total_pieces - 1) {
                                const size_t rem = e->size_bytes % e->piece_length;
                                if (rem != 0) next_piece_size = rem;
                            }
                            if (next_block_size > next_piece_size) next_block_size = next_piece_size;

                            request_block(poll_fds[i].fd, next_piece, 0, next_block_size);
                        } else {
                            goto drop_peer;
                        }
                    } else {
                        uint32_t next_block_size = DEFAULT_BLOCK_SIZE;
                        if (peers[i].current_block_offset + next_block_size > current_piece_size) {
                            next_block_size = current_piece_size - peers[i].current_block_offset;
                        }
                        request_block(poll_fds[i].fd, block_index, peers[i].current_block_offset, next_block_size);
                    }
                    continue;

                // 2 gotos in the same file X_X
                drop_peer:
                    if (peers[i].current_piece_assigned != -1) {
                        pthread_mutex_lock(&e->lock);
                        e->piece_states[peers[i].current_piece_assigned] = PIECE_MISSING;
                        pthread_mutex_unlock(&e->lock);
                    }
                    if (peers[i].piece_buffer) free(peers[i].piece_buffer);
                    close(poll_fds[i].fd);
                    poll_fds[i].fd = -1;
                    peers[i].state = PEER_STATE_DEAD;
                }
            }

            if (poll_fds[i].revents & POLLOUT) {
                if (peers[i].state == PEER_STATE_CONNECTING) {
                    int socket_error = 0;
                    socklen_t len = sizeof(socket_error);
                    getsockopt(poll_fds[i].fd, SOL_SOCKET, SO_ERROR, &socket_error, &len);

                    if (socket_error != 0) {
                        close(poll_fds[i].fd);
                        poll_fds[i].fd = -1;
                        peers[i].state = PEER_STATE_DEAD;
                        continue;
                    }

                    send(poll_fds[i].fd, &established_handshake, sizeof(PeerHandshake), 0);
                    peers[i].state = PEER_STATE_HANDSHAKING;
                    poll_fds[i].events = POLLIN;
                }
            }
        }
    }
}
