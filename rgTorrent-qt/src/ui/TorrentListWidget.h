#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QList>
#include "../backend/TorrentItem.h"

class QProgressBar;

class ProgressDelegate;

class TorrentListWidget final : public QWidget {
    Q_OBJECT
public:
    explicit TorrentListWidget(QWidget *parent = nullptr);

    void setTorrents(const QList<TorrentItem> &items);
    void setFilter(const QString &text);

    [[nodiscard]] int selectedId() const;

signals:
    void selectionChanged(const TorrentItem &item);
    void removeRequested(int id);

private slots:
    void onSelectionChanged();

private:
    QTableWidget         *m_table;
    QList<TorrentItem>    m_items;
    QList<TorrentItem>    m_filtered;
    QString               m_filter;
    ProgressDelegate     *m_progressDelegate;

    void rebuildTable();
    void applyFilter();

    static QString formatSize(quint64 bytes);
};
