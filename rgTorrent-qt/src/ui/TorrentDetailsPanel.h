#pragma once
#include <QWidget>
#include "../backend/TorrentItem.h"

class QLabel;
class QProgressBar;
class QFrame;

class TorrentDetailsPanel final : public QWidget {
    Q_OBJECT
public:
    explicit TorrentDetailsPanel(QWidget *parent = nullptr);
    void setTorrent(const TorrentItem &item) const;
    void clear();
    void retranslateUi() const;

signals:
    void closeRequested();

private:
    static void addRow(QWidget *grid, int row, const QString &key, QLabel **valueOut);

    QLabel *m_lName, *m_lSize, *m_lStatus, *m_lSeeds,
           *m_lPeers, *m_lProgress, *m_lSeeding, *m_lPath;
    QProgressBar *m_progressBar;
    QFrame       *m_card;
};
