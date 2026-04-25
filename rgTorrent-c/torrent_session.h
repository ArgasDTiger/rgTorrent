#ifndef TORRENT_SESSION_H
#define TORRENT_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TS_MAX_TORRENTS 256

typedef enum {
    TS_STATUS_DOWNLOADING,
    TS_STATUS_SEEDING,
    TS_STATUS_PAUSED,
    TS_STATUS_ERROR,
    TS_STATUS_QUEUED,
} TsStatus;

typedef struct TorrentSession TorrentSession;

TorrentSession *ts_create(void);
void            ts_destroy(TorrentSession *s);

int  ts_add_torrent(TorrentSession *s,
                    const char *torrent_path,
                    const char *save_path);
void ts_remove_torrent(TorrentSession *s, int id);
void ts_pause_torrent(TorrentSession *s, int id);
void ts_resume_torrent(TorrentSession *s, int id);

/* Stats — call these from TorrentBackend::poll() */
int         ts_torrent_count(const TorrentSession *s);
int         ts_torrent_id(const TorrentSession *s, int index);
const char *ts_torrent_name(const TorrentSession *s, int index);
uint64_t    ts_torrent_size(const TorrentSession *s, int index);
TsStatus    ts_torrent_status(const TorrentSession *s, int index);
const char *ts_torrent_status_str(const TorrentSession *s, int index);
int         ts_torrent_seeds(const TorrentSession *s, int index);
int         ts_torrent_peers(const TorrentSession *s, int index);
double      ts_torrent_progress(const TorrentSession *s, int index);
bool        ts_torrent_is_seeding(const TorrentSession *s, int index);
const char *ts_torrent_save_path(const TorrentSession *s, int index);
const char *ts_torrent_file_path(const TorrentSession *s, int index);

int ts_create_torrent(const char *source_dir,
                      const char *output_path,
                      const char *tracker_url);

#ifdef __cplusplus
}
#endif

#endif /* TORRENT_SESSION_H */
