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
    void retranslateUi() const;
    void clearSelection();

signals:
    void selectionChanged(const TorrentItem &item);
    void pauseRequested(int id);
    void resumeRequested(int id);
    void removeRequested();

private slots:
    void onSelectionChanged();
    void onCustomContextMenu(const QPoint &pos);
    void onDoubleClicked(int row, int col);

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
