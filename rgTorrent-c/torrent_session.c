#include "torrent_session.h"
#include "bencode_parser.h"
#include "bencoder.h"
#include "announce_connector.h"
#include "handshake.h"
#include "helpers.h"
#include "swarm.h"
#include "file_saver.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <openssl/sha.h>

TorrentSession *ts_create(void) {
    TorrentSession *s = calloc(1, sizeof(TorrentSession));
    pthread_mutex_init(&s->lock, NULL);
    s->next_id = 1;
    return s;
}

#define MAX_TRACKERS 30

typedef struct {
    char url[256];
    UdpAnnounceRequest req;
    char *result_peers;
    size_t result_len;
    pthread_t thread;
} TrackerJob;

static void *tracker_worker_thread(void *arg) {
    TrackerJob *job = arg;

    job->req.announce_address = job->url;

    job->result_peers = get_peers_list(&job->req, &job->result_len);
    return NULL;
}

void ts_destroy(TorrentSession *s) {
    // TODO: signal threads to stop, then join them
    pthread_mutex_destroy(&s->lock);
    free(s);
}

typedef struct {
    TorrentSession *session;
    int entry_index;
    BencodeNode *root;
} ThreadArgs;

static void *download_thread(void *arg) {
    ThreadArgs *targs = arg;
    TorrentSession *s = targs->session;
    const int idx = targs->entry_index;
    BencodeNode *root = targs->root;
    free(targs);

    TorrentEntry *e = &s->entries[idx];

    if (!root) {
        pthread_mutex_lock(&e->lock);
        e->status = TS_STATUS_ERROR;
        pthread_mutex_unlock(&e->lock);
        return NULL;
    }

    BencodeNode *infoNode = getDictValue(root, "info");
    if (!infoNode) {
        pthread_mutex_lock(&e->lock);
        e->status = TS_STATUS_ERROR;
        pthread_mutex_unlock(&e->lock);
        freeBencodeNode(root);
        return NULL;
    }

    BencodeNode *pieces_node = getDictValue(infoNode, "pieces");
    BencodeNode *piece_length_node = getDictValue(infoNode, "piece length");

    if (!pieces_node || pieces_node->type != BEN_STR ||
        pieces_node->string.length == 0 ||
        !piece_length_node || piece_length_node->type != BEN_INT) {
        pthread_mutex_lock(&e->lock);
        e->status = TS_STATUS_ERROR;
        pthread_mutex_unlock(&e->lock);
        freeBencodeNode(root);
        return NULL;
    }

    const size_t total_pieces = pieces_node->string.length / SHA_DIGEST_LENGTH;
    const size_t piece_length = piece_length_node->intValue;

    pthread_mutex_lock(&e->lock);
    e->total_pieces = total_pieces;
    e->piece_length = piece_length;

    e->piece_states = calloc(total_pieces, sizeof(uint8_t));
    e->pieces_completed = 0;
    pthread_mutex_unlock(&e->lock);
    pthread_mutex_unlock(&e->lock);

    {
        FILE *f = fopen(e->torrent_path, "rb");
        if (!f) {
            pthread_mutex_lock(&e->lock);
            e->status = TS_STATUS_ERROR;
            pthread_mutex_unlock(&e->lock);
            freeBencodeNode(root);
            return NULL;
        }
        const long info_len = infoNode->endOffset - infoNode->startOffset;
        char *info_buf = malloc(info_len);
        fseek(f, infoNode->startOffset, SEEK_SET);
        fread(info_buf, info_len, 1, f);
        fclose(f);
        SHA1((unsigned char *) info_buf, info_len, e->info_hash);
        free(info_buf);
    }

    const BencodeNode *announceNode = getDictValue(root, "announce");

    UdpAnnounceRequest req = {
        .announce_address = announceNode ? announceNode->string.data : NULL,
        .info_hash = e->info_hash,
        .peer_id = e->peer_id,
        .port = 6881,
        .uploaded = 0,
        .downloaded = 0,
        .left = (long) e->size_bytes,
    };

    TrackerJob jobs[MAX_TRACKERS];
    int job_count = 0;

    if (announceNode && announceNode->type == BEN_STR) {
        snprintf(jobs[job_count].url, sizeof(jobs[0].url), "%.*s",
                 (int)announceNode->string.length, announceNode->string.data);
        jobs[job_count].req = req;
        jobs[job_count].result_peers = NULL;
        job_count++;
    }

    const BencodeNode *ann_list = getDictValue(root, "announce-list");
    if (ann_list && ann_list->type == BEN_LIST) {
        for (size_t i = 0; i < ann_list->list.length && job_count < MAX_TRACKERS; i++) {
            const BencodeNode *tier = ann_list->list.items[i];
            if (!tier || tier->type != BEN_LIST) continue;
            for (size_t j = 0; j < tier->list.length && job_count < MAX_TRACKERS; j++) {
                const BencodeNode *url_node = tier->list.items[j];
                if (!url_node || url_node->type != BEN_STR) continue;

                snprintf(jobs[job_count].url, sizeof(jobs[0].url), "%.*s",
                         (int)url_node->string.length, url_node->string.data);
                jobs[job_count].req = req;
                jobs[job_count].result_peers = NULL;
                job_count++;
            }
        }
    }

    printf("[INFO] Spawning %d concurrent tracker requests...\n", job_count);

    for (int i = 0; i < job_count; i++) {
        pthread_create(&jobs[i].thread, NULL, tracker_worker_thread, &jobs[i]);
    }

    char *peers = NULL;
    size_t peers_len = 0;

    for (int i = 0; i < job_count; i++) {
        pthread_join(jobs[i].thread, NULL);

        if (jobs[i].result_peers) {
            if (!peers) {
                peers = jobs[i].result_peers;
                peers_len = jobs[i].result_len;
            } else {
                free(jobs[i].result_peers);
            }
        }
    }

    if (!peers) {
        fprintf(stderr, "[INFO] ALL trackers failed for %s\n", e->name);
        pthread_mutex_lock(&e->lock);
        e->status = TS_STATUS_ERROR;
        pthread_mutex_unlock(&e->lock);
        freeBencodeNode(root);
        return NULL;
    }

    printf("[INFO] Swarm located. Trying to establish connections.\n");

    if (!peers) {
        fprintf(stderr, "[thread] No peers found for %s\n", e->name);
        pthread_mutex_lock(&e->lock);
        e->status = TS_STATUS_ERROR;
        pthread_mutex_unlock(&e->lock);
        freeBencodeNode(root);
        return NULL;
    }

    const size_t peers_count = peers_len / 6;
    pthread_mutex_lock(&e->lock);
    e->peers_count = (int) peers_count;
    pthread_mutex_unlock(&e->lock);

    size_t num_files = 0;
    EndFile *end_files = fill_target_files(infoNode, &num_files, e->save_path);
    if (!end_files) {
        pthread_mutex_lock(&e->lock);
        e->status = TS_STATUS_ERROR;
        pthread_mutex_unlock(&e->lock);
        free(peers);
        freeBencodeNode(root);
        return NULL;
    }

    printf("[INFO] Verifying existing files for %s...\n", e->name);

    const unsigned char *pieces_hashes = pieces_node->string.data;
    unsigned char *verify_buffer = malloc(e->piece_length);
    int recovered_pieces = 0;

    for (size_t p = 0; p < e->total_pieces; p++) {
        size_t current_piece_size = e->piece_length;
        if (p == e->total_pieces - 1) {
            const size_t rem = e->size_bytes % e->piece_length;
            if (rem != 0) current_piece_size = rem;
        }

        if (read_piece_from_disk(p, e->piece_length, current_piece_size, verify_buffer, end_files, (int)num_files)) {
            unsigned char hash[SHA_DIGEST_LENGTH];
            SHA1(verify_buffer, current_piece_size, hash);
            const unsigned char *expected_hash = pieces_hashes + (p * SHA_DIGEST_LENGTH);

            if (memcmp(hash, expected_hash, SHA_DIGEST_LENGTH) == 0) {
                e->piece_states[p] = PIECE_DONE;
                recovered_pieces++;
            }
        }
    }
    free(verify_buffer);

    pthread_mutex_lock(&e->lock);
    e->pieces_completed = recovered_pieces;
    e->progress = (double)e->pieces_completed / (double)e->total_pieces;

    if (e->pieces_completed == e->total_pieces) {
        e->status = TS_STATUS_SEEDING;
        e->seeding = true;
    }
    pthread_mutex_unlock(&e->lock);

    printf("[INFO] Verification complete. Recovered %d / %ld pieces.\n", recovered_pieces, e->total_pieces);

    start_swarm(e, (unsigned char *) peers, peers_count, pieces_hashes, end_files, (int) num_files);

    free(end_files);
    free(peers);

    pthread_mutex_lock(&e->lock);
    if (e->pieces_completed == e->total_pieces) {
        e->status = TS_STATUS_SEEDING;
        e->seeding = true;
        e->progress = 1.0;
    }
    pthread_mutex_unlock(&e->lock);

    freeBencodeNode(root);
    return NULL;
}

int ts_add_torrent(TorrentSession *s,
                   const char *torrent_path,
                   const char *save_path) {
    pthread_mutex_lock(&s->lock);
    if (s->count >= TS_MAX_TORRENTS) {
        pthread_mutex_unlock(&s->lock);
        return -1;
    }

    const int idx = s->count++;
    TorrentEntry *e = &s->entries[idx];
    memset(e, 0, sizeof *e);
    pthread_mutex_init(&e->lock, NULL);

    e->id = s->next_id++;
    strncpy(e->torrent_path, torrent_path, sizeof e->torrent_path - 1);
    strncpy(e->save_path, save_path, sizeof e->save_path - 1);
    e->status = TS_STATUS_DOWNLOADING;

    rand_str(e->peer_id, 20);

    BencodeContext ctx = {0};
    ctx.file = fopen(torrent_path, "rb");
    BencodeNode *root = NULL;
    if (ctx.file) {
        root = parseCollectionValue(&ctx);
        if (root) {
            const BencodeNode *info = getDictValue(root, "info");
            if (info) {
                const BencodeNode *nameNode = getDictValue(info, "name");
                if (nameNode && nameNode->type == BEN_STR)
                    snprintf(e->name, sizeof e->name, "%.*s",
                             (int) nameNode->string.length,
                             nameNode->string.data);

                const BencodeNode *lenNode = getDictValue(info, "length");
                if (lenNode && lenNode->type == BEN_INT) {
                    e->size_bytes = (uint64_t) lenNode->intValue;
                } else {
                    const BencodeNode *files_list = getDictValue(info, "files");
                    if (files_list && files_list->type == BEN_LIST) {
                        uint64_t total_size = 0;
                        for (size_t i = 0; i < files_list->list.length; i++) {
                            const BencodeNode *file_dict = files_list->list.items[i];
                            const BencodeNode *flen = getDictValue(file_dict, "length");
                            if (flen && flen->type == BEN_INT) {
                                total_size += flen->intValue;
                            }
                        }
                        e->size_bytes = total_size;
                    }
                }
            }
        }
        fclose(ctx.file);
    }

    ThreadArgs *args = malloc(sizeof *args);
    args->session = s;
    args->entry_index = idx;
    args->root = root;
    pthread_create(&e->thread, NULL, download_thread, args);
    e->thread_running = true;

    const int id = e->id;
    pthread_mutex_unlock(&s->lock);
    return id;
}

void ts_remove_torrent(TorrentSession *s, int id) {
    pthread_mutex_lock(&s->lock);
    for (int i = 0; i < s->count; i++) {
        if (s->entries[i].id == id) {
            if (s->entries[i].thread_running)
                pthread_join(s->entries[i].thread, NULL);
            pthread_mutex_destroy(&s->entries[i].lock);

            if (s->entries[i].piece_states) {
                free(s->entries[i].piece_states);
            }

            memmove(&s->entries[i], &s->entries[i + 1],
                    (s->count - i - 1) * sizeof(TorrentEntry));
            s->count--;
            break;
        }
    }
    pthread_mutex_unlock(&s->lock);
}

int ts_create_torrent(const char *source_dir,
                      const char *output_path,
                      const char *tracker_url) {
    // TODO: implement torrent creation
    (void) source_dir;
    (void) output_path;
    (void) tracker_url;
    return 0;
}

int ts_torrent_count(const TorrentSession *s) { return s->count; }

int ts_torrent_id(const TorrentSession *s, const int index) {
    return s->entries[index].id;
}

const char *ts_torrent_name(const TorrentSession *s, const int index) {
    return s->entries[index].name;
}

uint64_t ts_torrent_size(const TorrentSession *s, const int index) {
    return s->entries[index].size_bytes;
}

double ts_torrent_progress(const TorrentSession *s, const int index) {
    TorrentEntry *e = (TorrentEntry *) &s->entries[index];
    pthread_mutex_lock(&e->lock);
    const double p = e->progress;
    pthread_mutex_unlock(&e->lock);
    return p;
}

TsStatus ts_torrent_status(const TorrentSession *s, const int index) {
    return s->entries[index].status;
}

const char *ts_torrent_status_str(const TorrentSession *s, int i) {
    switch (s->entries[i].status) {
        case TS_STATUS_DOWNLOADING: return "Downloading";
        case TS_STATUS_SEEDING: return "Seeding";
        case TS_STATUS_PAUSED: return "Paused";
        case TS_STATUS_ERROR: return "Error";
        case TS_STATUS_QUEUED: return "Queued";
        default: return "Unknown";
    }
}

int ts_torrent_seeds(const TorrentSession *s, const int index) { return s->entries[index].seeds; }
int ts_torrent_peers(const TorrentSession *s, const int index) { return s->entries[index].peers_count; }
bool ts_torrent_is_seeding(const TorrentSession *s, const int index) { return s->entries[index].seeding; }
const char *ts_torrent_save_path(const TorrentSession *s, const int index) { return s->entries[index].save_path; }
const char *ts_torrent_file_path(const TorrentSession *s, const int index) { return s->entries[index].torrent_path; }
