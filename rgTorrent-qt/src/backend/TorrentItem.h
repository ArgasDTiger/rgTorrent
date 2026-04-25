#pragma once
#include <QString>

struct TorrentItem {
    int     id          = 0;
    QString name;
    quint64 sizeBytes   = 0;
    QString status;
    int     seeds       = 0;
    int     peers       = 0;
    double  progress    = 0.0;
    bool    seeding     = false;
    QString savePath;
    QString torrentPath;
};
