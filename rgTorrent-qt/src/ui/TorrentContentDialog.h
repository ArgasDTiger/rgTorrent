#pragma once
#include <QDialog>

class TorrentContentDialog final : public QDialog {
    Q_OBJECT
public:
    explicit TorrentContentDialog(const QString &torrentPath,
                                   const QString &contents,
                                   QWidget *parent = nullptr);
};
