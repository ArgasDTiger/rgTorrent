#ifndef TORRENT_CREATOR_H
#define TORRENT_CREATOR_H

#ifdef __cplusplus
extern "C" {
#endif

    int ts_create_torrent(const char *source_dir,
                          const char *output_path,
                          const char *tracker_url,
                          int piece_length);

#ifdef __cplusplus
}
#endif

#endif /* TORRENT_CREATOR_H */