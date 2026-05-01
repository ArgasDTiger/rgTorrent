#include "TorrentBackend.h"
#include <QFileInfo>
#include <QSettings>
#include <QDateTime>

extern "C" {
#include "torrent_session.h"
#include "bencoder.h"
#include "bencode_parser.h"
}

static QString formatSize(const quint64 bytes) {
    if (bytes == 0) return QStringLiteral("—");
    const double kb = 1024, mb = kb * 1024, gb = mb * 1024;
    if (bytes >= gb) return QString("%1 GB").arg(bytes / gb, 0, 'f', 2);
    if (bytes >= mb) return QString("%1 MB").arg(bytes / mb, 0, 'f', 1);
    if (bytes >= kb) return QString("%1 KB").arg(bytes / kb, 0, 'f', 0);
    return QString("%1 B").arg(bytes);
}

static QString statusToString(const TsStatus s) {
    switch (s) {
        case TS_STATUS_DOWNLOADING: return TorrentBackend::tr("Downloading");
        case TS_STATUS_SEEDING:     return TorrentBackend::tr("Seeding");
        case TS_STATUS_PAUSED:      return TorrentBackend::tr("Paused");
        case TS_STATUS_ERROR:       return TorrentBackend::tr("Error");
        case TS_STATUS_QUEUED:      return TorrentBackend::tr("Queued");
        default:                    return TorrentBackend::tr("Unknown");
    }
}

TorrentBackend::TorrentBackend(QObject *parent)
    : QObject(parent) {
    m_session = ts_create();

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, &TorrentBackend::poll);
    m_pollTimer->start();
}

TorrentBackend::~TorrentBackend() {
    ts_destroy(m_session);
}

void TorrentBackend::poll() {
    QList<TorrentItem> fresh;
    const int count = ts_torrent_count(m_session);

    for (int i = 0; i < count; ++i) {
        TorrentItem t;
        t.id = ts_torrent_id(m_session, i);
        t.name = QString::fromUtf8(ts_torrent_name(m_session, i));
        t.sizeBytes = ts_torrent_size(m_session, i);
        t.status = statusToString(ts_torrent_status(m_session, i));
        t.seeds       = ts_torrent_seeds(m_session, i);
        t.totalSeeds  = ts_torrent_total_seeds(m_session, i);
        t.peers       = ts_torrent_peers(m_session, i);
        t.totalPeers  = ts_torrent_total_peers(m_session, i);
        t.progress = ts_torrent_progress(m_session, i);
        t.seeding = ts_torrent_is_seeding(m_session, i);
        t.savePath = QString::fromUtf8(ts_torrent_save_path(m_session, i));
        t.torrentPath = QString::fromUtf8(ts_torrent_file_path(m_session, i));
        fresh.append(t);
    }

    emit torrentsChanged(fresh);
}

QList<TorrentItem> TorrentBackend::torrents() const {
    QList<TorrentItem> result;
    const int count = ts_torrent_count(m_session);
    for (int i = 0; i < count; ++i) {
        TorrentItem t;
        t.id = ts_torrent_id(m_session, i);
        t.name = QString::fromUtf8(ts_torrent_name(m_session, i));
        t.sizeBytes = ts_torrent_size(m_session, i);
        t.status = statusToString(ts_torrent_status(m_session, i));
        t.seeds = ts_torrent_seeds(m_session, i);
        t.peers = ts_torrent_peers(m_session, i);
        t.progress = ts_torrent_progress(m_session, i);
        t.seeding = ts_torrent_is_seeding(m_session, i);
        t.savePath = QString::fromUtf8(ts_torrent_save_path(m_session, i));
        t.torrentPath = QString::fromUtf8(ts_torrent_file_path(m_session, i));
        result.append(t);
    }
    return result;
}

void TorrentBackend::addTorrent(const QString &torrentPath, const QString &savePath) {
    const int id = ts_add_torrent(m_session,
                                  torrentPath.toUtf8().constData(),
                                  savePath.toUtf8().constData());
    if (id < 0) {
        emit errorOccurred(tr("Failed to add torrent: %1").arg(torrentPath));
        return;
    }
    // poll() will pick up the new entry on its next tick (within 1 second).
    // Trigger an immediate refresh so the UI responds instantly.
    poll();
}

void TorrentBackend::removeTorrent(const int id) {
    ts_remove_torrent(m_session, id);
    poll();
}

void TorrentBackend::createTorrent(const QString &sourceDir,
                                   const QString &outputPath,
                                   const QString &trackerUrl) {
    const int rc = ts_create_torrent(sourceDir.toUtf8().constData(),
                                     outputPath.toUtf8().constData(),
                                     trackerUrl.toUtf8().constData());
    if (rc != 0)
        emit errorOccurred(tr("Failed to create torrent."));
}

QString TorrentBackend::torrentContents(const QString &torrentPath) {
    BencodeContext ctx = {};
    ctx.file = fopen(torrentPath.toUtf8().constData(), "rb");
    if (!ctx.file)
        return tr("Cannot open file: %1").arg(torrentPath);

    BencodeNode *root = parseCollectionValue(&ctx);
    fclose(ctx.file);

    if (!root || ctx.hasError)
        return tr("Parse error: %1").arg(QString::fromUtf8(ctx.errorMsg));

    QString out;

    const BencodeNode *info = getDictValue(root, "info");
    if (info) {
        if (const BencodeNode *nameNode = getDictValue(info, "name"); nameNode && nameNode->type == BEN_STR)
            out += tr("Name:") + "         " +
                   QString::fromUtf8((char*)nameNode->string.data,
                                     (int)nameNode->string.length) + "\n";

        const BencodeNode *lenNode = getDictValue(info, "length");
        if (lenNode && lenNode->type == BEN_INT)
            out += tr("Total Size:") + "   " + formatSize(lenNode->intValue) + "\n";

        if (const BencodeNode *plNode = getDictValue(info, "piece length"); plNode && plNode->type == BEN_INT)
            out += tr("Piece Length:") + " " + formatSize(plNode->intValue) + "\n";

        if (const BencodeNode *piecesNode = getDictValue(info, "pieces"); piecesNode && piecesNode->type == BEN_STR)
            out += tr("Pieces:") + "       " +
                   QString::number(piecesNode->string.length / 20) + "\n";

        if (const BencodeNode *privNode = getDictValue(info, "private"); privNode && privNode->type == BEN_INT) {
            out += tr("Private:") + "      " + (privNode->intValue == 1 ? tr("Yes") : tr("No")) + "\n";
        }

        const BencodeNode *files = getDictValue(info, "files");
        if (files && files->type == BEN_LIST) {
            out += "\n" + tr("Files (%1):").arg(files->list.length) + "\n";
            for (size_t i = 0; i < files->list.length; ++i) {
                const BencodeNode *file = files->list.items[i];
                const BencodeNode *pathList = getDictValue(file, "path");
                const BencodeNode *fileLen = getDictValue(file, "length");

                QString fullPath;
                if (pathList && pathList->type == BEN_LIST) {
                    for (size_t j = 0; j < pathList->list.length; ++j) {
                        const BencodeNode *seg = pathList->list.items[j];
                        if (seg->type == BEN_STR) {
                            if (!fullPath.isEmpty()) fullPath += "/";
                            fullPath += QString::fromUtf8((char *) seg->string.data, (int) seg->string.length);
                        }
                    }
                }

                QString sizeStr = (fileLen && fileLen->type == BEN_INT) ? formatSize(fileLen->intValue) : "Unknown";
                out += "  " + fullPath + " (" + sizeStr + ")\n";
            }
        }
    }

    if (const BencodeNode *ann = getDictValue(root, "announce"); ann && ann->type == BEN_STR)
        out += "\n" + tr("Primary Tracker:") + " " +
               QString::fromUtf8(reinterpret_cast<char *>(ann->string.data),
                                 static_cast<int>(ann->string.length)) + "\n";

    if (const BencodeNode *annList = getDictValue(root, "announce-list"); annList && annList->type == BEN_LIST) {
        out += tr("Backup Trackers:") + "\n";
        for (size_t i = 0; i < annList->list.length; ++i) {
            if (const BencodeNode *tier = annList->list.items[i]; tier->type == BEN_LIST) {
                for (size_t j = 0; j < tier->list.length; ++j) {
                    if (const BencodeNode *url = tier->list.items[j]; url->type == BEN_STR) {
                        out += "  " + QString::fromUtf8(reinterpret_cast<char *>(url->string.data),
                                                        static_cast<int>(url->string.length)) + "\n";
                    }
                }
            }
        }
    }

    if (const BencodeNode *creationDate = getDictValue(root, "creation date"); creationDate && creationDate->type == BEN_INT) {
        const QDateTime dt = QDateTime::fromSecsSinceEpoch(creationDate->intValue);
        out += "\n" + tr("Created on:") + "   " + dt.toString("yyyy-MM-dd HH:mm:ss") + "\n";
    }

    if (const BencodeNode *comment = getDictValue(root, "comment"); comment && comment->type == BEN_STR)
        out += tr("Comment:") + "      " +
               QString::fromUtf8(reinterpret_cast<char *>(comment->string.data),
                                 static_cast<int>(comment->string.length)) + "\n";

    if (const BencodeNode *createdBy = getDictValue(root, "created by"); createdBy && createdBy->type == BEN_STR)
        out += tr("Created by:") + "   " +
               QString::fromUtf8(reinterpret_cast<char *>(createdBy->string.data),
                                 static_cast<int>(createdBy->string.length)) + "\n";

    freeBencodeNode(root);
    return out;
}

void TorrentBackend::saveSession() const {
    QSettings settings("rgTorrent", "rgTorrent");
    QStringList activeTorrents;

    const int count = ts_torrent_count(m_session);
    for (int i = 0; i < count; ++i) {
        QString tPath = QString::fromUtf8(ts_torrent_file_path(m_session, i));
        QString sPath = QString::fromUtf8(ts_torrent_save_path(m_session, i));

        activeTorrents.append(tPath + "|" + sPath);
    }

    settings.setValue("session/torrents", activeTorrents);
}

void TorrentBackend::loadSession() const {
    const QSettings settings("rgTorrent", "rgTorrent");
    QStringList activeTorrents = settings.value("session/torrents").toStringList();

    for (const QString &entry: activeTorrents) {
        if (QStringList parts = entry.split("|"); parts.size() == 2) {
            ts_add_torrent(m_session,
                           parts[0].toUtf8().constData(),
                           parts[1].toUtf8().constData());
        }
    }
}
