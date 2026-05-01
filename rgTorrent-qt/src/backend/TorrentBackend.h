#pragma once
#include <QObject>
#include <QTimer>
#include <QList>
#include <QString>
#include "TorrentItem.h"

extern "C" {
#include "torrent_session.h"
}

class TorrentBackend final : public QObject {
    Q_OBJECT
public:
    explicit TorrentBackend(QObject *parent = nullptr);
    ~TorrentBackend() override;

    void loadSession() const;
    void saveSession() const;

    [[nodiscard]] QList<TorrentItem> torrents() const;

    static QString torrentContents(const QString &torrentPath);

public slots:
    void addTorrent(const QString &torrentPath, const QString &savePath);
    void removeTorrent(int id);
    void createTorrent(const QString &sourceDir, const QString &outputPath,
                       const QString &trackerUrl);

    signals:
        void torrentsChanged(const QList<TorrentItem> &items);
    void errorOccurred(const QString &message);

private slots:
    void poll();

private:
    QTimer             *m_pollTimer = nullptr;
    TorrentSession     *m_session   = nullptr;
};