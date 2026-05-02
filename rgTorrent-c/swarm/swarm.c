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
#define UNCHOKE 1
#define BITTORENT_PROTOCOL "BitTorrent protocol"
#define MAX_SEED_BLOCK_LENGTH 131072

static void set_nonblocking(const int sockfd) {
    const int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
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
                struct pollfd pfd = {.fd = fd, .events = POLLIN};
                if (poll(&pfd, 1, 15000) <= 0) return false;
            } else {
                return false;
            }
        }
    }
    return true;
}

void request_block(const int sockfd, const uint32_t piece_index, const uint32_t block_offset, const uint32_t block_length) {
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

static void drop_peer(TorrentEntry *e, struct pollfd *pfd, PeerConnection *peer) {
    if (peer->current_piece_assigned != -1) {
        pthread_mutex_lock(&e->lock);
        e->piece_states[peer->current_piece_assigned] = PIECE_MISSING;
        pthread_mutex_unlock(&e->lock);
        peer->current_piece_assigned = -1;
    }
    if (peer->piece_buffer) {
        free(peer->piece_buffer);
        peer->piece_buffer = NULL;
    }
    if (peer->inventory) {
        free(peer->inventory);
        peer->inventory = NULL;
    }
    if (pfd->fd != -1) {
        close(pfd->fd);
        pfd->fd = -1;
    }
    peer->state = PEER_STATE_DEAD;
}

static bool handle_handshake(const TorrentEntry *e, const struct pollfd *pfd, PeerConnection *peer) {
    PeerHandshake peer_reply;
    const ssize_t received = recv(pfd->fd, &peer_reply, sizeof(PeerHandshake), 0);

    if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
    if (received != sizeof(PeerHandshake) || memcmp(peer_reply.info_hash, e->info_hash, 20) != 0) return false;

    peer->state = PEER_STATE_WAITING_BITFIELD;
    return true;
}

static bool handle_bitfield(const TorrentEntry *e, const struct pollfd *pfd, PeerConnection *peer) {
    uint32_t msg_len_net = 0;
    const ssize_t recvd = recv(pfd->fd, &msg_len_net, 4, 0);

    if (recvd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
    if (recvd <= 0) return false;
    if (recvd < 4 && !read_exactly(pfd->fd, (char *) &msg_len_net + recvd, 4 - recvd)) return false;

    const uint32_t msg_len = ntohl(msg_len_net);
    peer->inventory = calloc(e->total_pieces, sizeof(bool));

    if (msg_len > 0) {
        uint8_t msg_id;
        if (!read_exactly(pfd->fd, &msg_id, 1)) return false;

        const uint32_t payload_len = msg_len - 1;
        if (msg_id == 5 && payload_len > 0) {
            unsigned char *payload = malloc(payload_len);
            if (read_exactly(pfd->fd, payload, payload_len)) {
                for (size_t p = 0; p < e->total_pieces; p++) {
                    const size_t byte_index = p / 8;
                    const size_t bit_index = 7 - (p % 8);
                    if (byte_index < payload_len && payload[byte_index] >> bit_index & 1) {
                        peer->inventory[p] = true;
                    }
                }
            }
            free(payload);
        } else if (payload_len > 0) {
            unsigned char *temp = malloc(payload_len);
            read_exactly(pfd->fd, temp, payload_len);
            free(temp);
        }
    }

    const uint8_t interested_msg[5] = {0, 0, 0, 1, 2};
    send(pfd->fd, interested_msg, 5, 0);
    peer->state = PEER_STATE_WAITING_UNCHOKE;
    return true;
}

static bool handle_unchoke(TorrentEntry *e, const struct pollfd *pfd, PeerConnection *peer) {
    uint32_t msg_len_net = 0;
    const ssize_t recvd = recv(pfd->fd, &msg_len_net, 4, 0);

    if (recvd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
    if (recvd <= 0) return false;
    if (recvd < 4 && !read_exactly(pfd->fd, (char *) &msg_len_net + recvd, 4 - recvd)) return false;

    const uint32_t msg_len = ntohl(msg_len_net);

    // keep-alive
    if (msg_len == 0) return true;

    uint8_t msg_id;
    if (!read_exactly(pfd->fd, &msg_id, 1)) return false;

    const uint32_t payload_len = msg_len - 1;
    if (payload_len > 0) {
        unsigned char *temp = malloc(payload_len);
        read_exactly(pfd->fd, temp, payload_len);
        free(temp);
    }

    if (msg_id == UNCHOKE) {
        const int next_piece = get_next_piece_to_download(e, peer->inventory);
        if (next_piece == -1) return false;

        peer->current_piece_assigned = next_piece;
        peer->current_block_offset = 0;
        peer->piece_buffer = malloc(e->piece_length);

        uint32_t block_size = DEFAULT_BLOCK_SIZE;
        size_t current_piece_size = e->piece_length;
        if (next_piece == (int) e->total_pieces - 1) {
            const size_t remainder = e->size_bytes % e->piece_length;
            if (remainder != 0) current_piece_size = remainder;
        }
        if (peer->current_block_offset + block_size > current_piece_size) {
            block_size = current_piece_size - peer->current_block_offset;
        }

        request_block(pfd->fd, next_piece, 0, block_size);
        peer->state = PEER_STATE_DOWNLOADING;
    }
    return true;
}

static bool handle_downloading(TorrentEntry *e, const struct pollfd *pfd, PeerConnection *peer,
                               const unsigned char *pieces_hashes, const EndFile *end_files, const int num_files) {
    uint32_t msg_len_net;
    const ssize_t res = recv(pfd->fd, &msg_len_net, 4, 0);

    if (res < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
    if (res <= 0) return false;
    if (res < 4 && !read_exactly(pfd->fd, (char *) &msg_len_net + res, 4 - res)) return false;

    const uint32_t msg_len = ntohl(msg_len_net);
    if (msg_len == 0) return true; // keep-alive

    uint8_t msg_id;
    if (!read_exactly(pfd->fd, &msg_id, 1)) return false;

    const uint32_t payload_len = msg_len - 1;
    if (msg_id == 6) {
        uint32_t net_index, net_begin, net_length;
        if (!read_exactly(pfd->fd, &net_index, 4)) return false;
        if (!read_exactly(pfd->fd, &net_begin, 4)) return false;
        if (!read_exactly(pfd->fd, &net_length, 4)) return false;

        const uint32_t block_index = ntohl(net_index);
        const uint32_t block_begin = ntohl(net_begin);
        const uint32_t block_length = ntohl(net_length);

        pthread_mutex_lock(&e->lock);
        const bool has_piece = (e->piece_states[block_index] == PIECE_DONE);
        pthread_mutex_unlock(&e->lock);

        if (has_piece && block_length <= MAX_SEED_BLOCK_LENGTH) {
            size_t current_piece_size = e->piece_length;
            if (block_index == e->total_pieces - 1) {
                const size_t rem = e->size_bytes % e->piece_length;
                if (rem != 0) current_piece_size = rem;
            }

            unsigned char *piece_buf = malloc(current_piece_size);

            if (read_piece_from_disk(block_index, e->piece_length, current_piece_size, piece_buf, end_files, num_files)) {
                if (block_begin + block_length <= current_piece_size) {
                    const uint32_t out_msg_len = htonl(9 + block_length);
                    const uint8_t out_id = 7;
                    unsigned char header[13];

                    memcpy(header, &out_msg_len, 4);
                    header[4] = out_id;
                    memcpy(header + 5, &net_index, 4);
                    memcpy(header + 9, &net_begin, 4);

                    send(pfd->fd, header, 13, 0);
                    send(pfd->fd, piece_buf + block_begin, block_length, 0);
                }
            }
            free(piece_buf);
        }
        return true;
    }

    if (msg_id != 7) {
        if (payload_len > 0) {
            unsigned char *temp = malloc(payload_len);
            read_exactly(pfd->fd, temp, payload_len);
            free(temp);
        }
        return true;
    }

    uint32_t net_index, net_begin;
    if (!read_exactly(pfd->fd, &net_index, 4)) return false;
    if (!read_exactly(pfd->fd, &net_begin, 4)) return false;

    const uint32_t block_index = ntohl(net_index);
    const uint32_t block_begin = ntohl(net_begin);
    const uint32_t block_data_len = payload_len - 8;

    if (block_index != peer->current_piece_assigned) return false;

    unsigned char *block_data = malloc(block_data_len);
    if (!read_exactly(pfd->fd, block_data, block_data_len)) {
        free(block_data);
        return false;
    }

    memcpy(peer->piece_buffer + block_begin, block_data, block_data_len);
    free(block_data);
    peer->current_block_offset += block_data_len;

    size_t current_piece_size = e->piece_length;
    if (block_index == (int) e->total_pieces - 1) {
        const size_t remainder = e->size_bytes % e->piece_length;
        if (remainder != 0) current_piece_size = remainder;
    }

    if (peer->current_block_offset >= current_piece_size) {
        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1(peer->piece_buffer, current_piece_size, hash);
        const unsigned char *expected_hash = pieces_hashes + block_index * SHA_DIGEST_LENGTH;

        // corrupt piece
        if (memcmp(hash, expected_hash, SHA_DIGEST_LENGTH) != 0) return false;

        write_piece_to_disk(block_index, e->piece_length, peer->piece_buffer, end_files, num_files);

        pthread_mutex_lock(&e->lock);
        e->piece_states[block_index] = PIECE_DONE;
        e->pieces_completed++;
        e->progress = (double) e->pieces_completed / (double) e->total_pieces;
        pthread_mutex_unlock(&e->lock);

        free(peer->piece_buffer);
        peer->piece_buffer = NULL;

        const int next_piece = get_next_piece_to_download(e, peer->inventory);
        if (next_piece != -1) {
            peer->current_piece_assigned = next_piece;
            peer->current_block_offset = 0;
            peer->piece_buffer = malloc(e->piece_length);

            uint32_t next_block_size = DEFAULT_BLOCK_SIZE;
            size_t next_piece_size = e->piece_length;
            if (next_piece == (int) e->total_pieces - 1) {
                const size_t rem = e->size_bytes % e->piece_length;
                if (rem != 0) next_piece_size = rem;
            }
            if (next_block_size > next_piece_size) next_block_size = next_piece_size;

            request_block(pfd->fd, next_piece, 0, next_block_size);
        } else {
            peer->current_piece_assigned = -1;
            return true;
        }
    } else {
        uint32_t next_block_size = DEFAULT_BLOCK_SIZE;
        if (peer->current_block_offset + next_block_size > current_piece_size) {
            next_block_size = current_piece_size - peer->current_block_offset;
        }
        request_block(pfd->fd, block_index, peer->current_block_offset, next_block_size);
    }
    return true;
}

static void initiate_connections(struct pollfd *poll_fds, PeerConnection *peers, const unsigned char *peers_list, size_t peers_count) {
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
}

void start_swarm(TorrentEntry *e, const unsigned char *peers_list, const size_t peers_count,
                 const unsigned char *pieces_hashes, const EndFile *end_files, const int num_files) {
    // +1 is to hold server socket
    struct pollfd poll_fds[MAX_PEERS + 1];
    PeerConnection peers[MAX_PEERS];

    for (int i = 0; i < MAX_PEERS; i++) {
        poll_fds[i].fd = -1;
        peers[i].state = PEER_STATE_DEAD;
        peers[i].inventory = NULL;
        peers[i].current_piece_assigned = -1;
        peers[i].piece_buffer = NULL;
    }

    initiate_connections(poll_fds, peers, peers_list, peers_count);

    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    const int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(server_fd);

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int bound_port = 6881;
    while (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 && bound_port < 6890) {
        bound_port++;
        server_addr.sin_port = htons(bound_port);
    }

    listen(server_fd, 10);
    poll_fds[MAX_PEERS].fd = server_fd;
    poll_fds[MAX_PEERS].events = POLLIN;

    printf("[INFO] Listening for incoming connections on port %d\n", bound_port);

    PeerHandshake established_handshake;
    established_handshake.pstrlen = 19;
    memcpy(established_handshake.pstr, BITTORENT_PROTOCOL, 19);
    memset(established_handshake.reserved, 0, 8);
    memcpy(established_handshake.info_hash, e->info_hash, 20);
    memcpy(established_handshake.peer_id, e->peer_id, 20);

    while (true) {
        pthread_mutex_lock(&e->lock);
        TsStatus current_status = e->status;
        pthread_mutex_unlock(&e->lock);

        if (current_status == TS_STATUS_ERROR) break;

        if (current_status == TS_STATUS_PAUSED) {
            for (int i = 0; i < MAX_PEERS; i++) {
                drop_peer(e, &poll_fds[i], &peers[i]);
            }

            // TODO: probably only temp solution
            while (true) {
                sleep(1);
                pthread_mutex_lock(&e->lock);
                current_status = e->status;
                pthread_mutex_unlock(&e->lock);
                if (current_status != TS_STATUS_PAUSED) break;
            }

            if (current_status == TS_STATUS_DOWNLOADING || current_status == TS_STATUS_SEEDING) {
                initiate_connections(poll_fds, peers, peers_list, peers_count);
            }
            continue;
        }

        const int activity = poll(poll_fds, MAX_PEERS, 1000);
        if (activity < 0) break;

        int live_seeds = 0;
        int live_peers = 0;

        for (int i = 0; i < MAX_PEERS; i++) {
            if (peers[i].state >= PEER_STATE_WAITING_UNCHOKE && peers[i].inventory != NULL) {
                bool is_seed = true;

                for (size_t p = 0; p < e->total_pieces; p++) {
                    if (!peers[i].inventory[p]) {
                        is_seed = false;
                        break;
                    }
                }

                if (is_seed) live_seeds++;
                else live_peers++;
            }
        }

        pthread_mutex_lock(&e->lock);
        e->seeds = live_seeds;
        e->peers_count = live_peers;
        pthread_mutex_unlock(&e->lock);

        if (activity == 0) continue;
        if (poll_fds[MAX_PEERS].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            const int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

            if (new_fd >= 0) {
                set_nonblocking(new_fd);
                bool slot_found = false;
                for (int i = 0; i < MAX_PEERS; i++) {
                    if (peers[i].state == PEER_STATE_DEAD) {
                        poll_fds[i].fd = new_fd;
                        poll_fds[i].events = POLLIN | POLLOUT;
                        peers[i].sockfd = new_fd;
                        peers[i].state = PEER_STATE_INCOMING_HANDSHAKE;
                        slot_found = true;
                        printf("[Swarm] Accepted incoming peer connection!\n");
                        break;
                    }
                }
                if (!slot_found) close(new_fd);
            }
        }

        for (int i = 0; i < MAX_PEERS; i++) {
            if (poll_fds[i].fd == -1) continue;

            if (poll_fds[i].revents & POLLIN) {
                bool keep_alive = true;

                switch (peers[i].state) {
                    case PEER_STATE_HANDSHAKING:
                        keep_alive = handle_handshake(e, &poll_fds[i], &peers[i]);
                        break;
                    case PEER_STATE_WAITING_BITFIELD:
                        keep_alive = handle_bitfield(e, &poll_fds[i], &peers[i]);
                        break;
                    case PEER_STATE_WAITING_UNCHOKE:
                        keep_alive = handle_unchoke(e, &poll_fds[i], &peers[i]);
                        break;
                    case PEER_STATE_DOWNLOADING:
                        keep_alive = handle_downloading(e, &poll_fds[i], &peers[i], pieces_hashes, end_files, num_files);
                        break;
                    case PEER_STATE_INCOMING_HANDSHAKE: {
                        PeerHandshake incoming;
                        const ssize_t recvd = recv(poll_fds[i].fd, &incoming, sizeof(PeerHandshake), 0);
                        if (recvd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            keep_alive = true;
                            break;
                        }
                        if (recvd != sizeof(PeerHandshake) || memcmp(incoming.info_hash, e->info_hash, 20) != 0) {
                            keep_alive = false;
                        } else {
                            send(poll_fds[i].fd, &established_handshake, sizeof(PeerHandshake), 0);

                            const uint32_t bitfield_len = (e->total_pieces + 7) / 8;
                            uint32_t bitfield_msg_len = htonl(1 + bitfield_len);
                            const uint8_t msg_id_bitfield = 5;
                            unsigned char *bitfield_msg = calloc(1, 5 + bitfield_len);
                            memcpy(bitfield_msg, &bitfield_msg_len, 4);
                            bitfield_msg[4] = msg_id_bitfield;
                            pthread_mutex_lock(&e->lock);
                            for (size_t p = 0; p < e->total_pieces; p++) {
                                if (e->piece_states[p] == PIECE_DONE) bitfield_msg[5 + (p / 8)] |= (1 << (7 - (p % 8)));
                            }
                            pthread_mutex_unlock(&e->lock);
                            send(poll_fds[i].fd, bitfield_msg, 5 + bitfield_len, 0);
                            free(bitfield_msg);

                            const uint8_t unchoke_msg[5] = {0, 0, 0, 1, 1};
                            send(poll_fds[i].fd, unchoke_msg, 5, 0);

                            peers[i].state = PEER_STATE_WAITING_BITFIELD;
                        }
                        break;
                    }
                    default:
                        break;
                }

                if (!keep_alive) {
                    drop_peer(e, &poll_fds[i], &peers[i]);
                }
            }

            if (poll_fds[i].revents & POLLOUT) {
                if (peers[i].state == PEER_STATE_CONNECTING) {
                    int socket_error = 0;
                    socklen_t len = sizeof(socket_error);
                    getsockopt(poll_fds[i].fd, SOL_SOCKET, SO_ERROR, &socket_error, &len);

                    if (socket_error != 0) {
                        drop_peer(e, &poll_fds[i], &peers[i]);
                        continue;
                    }

                    send(poll_fds[i].fd, &established_handshake, sizeof(PeerHandshake), 0);

                    const uint32_t bitfield_len = (e->total_pieces + 7) / 8;
                    uint32_t bitfield_msg_len = htonl(1 + bitfield_len);
                    const uint8_t msg_id_bitfield = 5;
                    unsigned char *bitfield_msg = calloc(1, 5 + bitfield_len);

                    memcpy(bitfield_msg, &bitfield_msg_len, 4);
                    bitfield_msg[4] = msg_id_bitfield;

                    pthread_mutex_lock(&e->lock);
                    for (size_t p = 0; p < e->total_pieces; p++) {
                        if (e->piece_states[p] == PIECE_DONE) {
                            bitfield_msg[5 + (p / 8)] |= (1 << (7 - (p % 8)));
                        }
                    }
                    pthread_mutex_unlock(&e->lock);

                    send(poll_fds[i].fd, bitfield_msg, 5 + bitfield_len, 0);
                    free(bitfield_msg);
                    const uint8_t unchoke_msg[5] = {0, 0, 0, 1, 1}; // Len=1, ID=1
                    send(poll_fds[i].fd, unchoke_msg, 5, 0);

                    peers[i].state = PEER_STATE_HANDSHAKING;
                    poll_fds[i].events = POLLIN;
                }
            }
        }
    }

    printf("[INFO] Shutting down network threads for info hash...\n");

    if (poll_fds[MAX_PEERS].fd != -1) {
        close(poll_fds[MAX_PEERS].fd);
        poll_fds[MAX_PEERS].fd = -1;
    }

     for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].state != PEER_STATE_DEAD) {
            drop_peer(e, &poll_fds[i], &peers[i]);
        }
    }
}