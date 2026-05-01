#ifndef TORRENT_SESSION_H
#define TORRENT_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <bits/pthreadtypes.h>

#ifdef __cplusplus
extern "C" {


#endif

#define TS_MAX_TORRENTS 256
#define DEFAULT_BLOCK_SIZE 16384

typedef enum {
    TS_STATUS_VERIFYING,
    TS_STATUS_DOWNLOADING,
    TS_STATUS_SEEDING,
    TS_STATUS_PAUSED,
    TS_STATUS_ERROR,
    TS_STATUS_QUEUED,
} TsStatus;

typedef struct TorrentEntry {
    int id;
    char torrent_path[512];
    char save_path[512];
    char name[256];
    uint64_t size_bytes;
    double progress;
    int seeds;
    int total_seeds;
    int peers_count;
    int total_peers;
    bool seeding;
    TsStatus status;
    pthread_t thread;
    bool thread_running;
    pthread_mutex_t lock; // protects progress/status/seeds/peers

    uint8_t info_hash[20];
    uint8_t peer_id[20];
    size_t total_pieces;
    size_t piece_length;
    uint8_t *piece_states;
    size_t pieces_completed;
} TorrentEntry;

struct TorrentSession {
    TorrentEntry entries[TS_MAX_TORRENTS];
    int count;
    int next_id;
    pthread_mutex_t lock;
};

typedef struct TorrentSession TorrentSession;

TorrentSession *ts_create(void);

void ts_destroy(TorrentSession *s);

int ts_add_torrent(TorrentSession *s,
                   const char *torrent_path,
                   const char *save_path);

void ts_remove_torrent(TorrentSession *s, int id);

void ts_pause_torrent(TorrentSession *s, int id);

void ts_resume_torrent(TorrentSession *s, int id);

int ts_torrent_count(const TorrentSession *s);

int ts_torrent_id(const TorrentSession *s, int index);

const char *ts_torrent_name(const TorrentSession *s, int index);

uint64_t ts_torrent_size(const TorrentSession *s, int index);

TsStatus ts_torrent_status(const TorrentSession *s, int index);

const char *ts_torrent_status_str(const TorrentSession *s, int index);

int ts_torrent_seeds(const TorrentSession *s, int index);

int ts_torrent_peers(const TorrentSession *s, int index);

double ts_torrent_progress(const TorrentSession *s, int index);

bool ts_torrent_is_seeding(const TorrentSession *s, int index);

const char *ts_torrent_save_path(const TorrentSession *s, int index);

const char *ts_torrent_file_path(const TorrentSession *s, int index);

int ts_create_torrent(const char *source_dir,
                      const char *output_path,
                      const char *tracker_url);

int ts_torrent_total_seeds(const TorrentSession *s, int index);

int ts_torrent_total_peers(const TorrentSession *s, int index);

#ifdef __cplusplus
}
#endif

#endif /* TORRENT_SESSION_H */
