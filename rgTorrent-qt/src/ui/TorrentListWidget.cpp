#include "TorrentListWidget.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyleOptionProgressBar>
#include <QApplication>

class ProgressDelegate final : public QStyledItemDelegate {
public:
    explicit ProgressDelegate(QObject *p = nullptr) : QStyledItemDelegate(p) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        double pct = index.data(Qt::UserRole).toDouble();

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        QRect r = option.rect.adjusted(8, (option.rect.height()-6)/2,
                                       -8, -(option.rect.height()-6)/2);

        QColor track = (option.state & QStyle::State_Selected)
                           ? QColor("#2a2d44") : QColor("#1e2030");
        // In light mode the colours look wrong if we just hardcode dark ones;
        // peek at the background to decide:
        if (QColor bg = option.palette.window().color(); bg.lightness() > 128)
            track = (option.state & QStyle::State_Selected)
                        ? QColor("#c5cae9") : QColor("#dde0f0");

        painter->setBrush(track);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(r, 3, 3);

        if (pct > 0.0) {
            QRect fill = r;
            fill.setWidth(qRound(r.width() * pct));
            QLinearGradient g(fill.topLeft(), fill.topRight());
            g.setColorAt(0, QColor("#5c6bc0"));
            g.setColorAt(1, QColor("#7986cb"));
            painter->setBrush(g);
            painter->drawRoundedRect(fill, 3, 3);
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return {120, 32};
    }
};

enum Col {
    COL_NAME=0,
    COL_SIZE,
    COL_STATUS,
    COL_SEEDS,
    COL_PEERS,
    COL_PROGRESS,
    COL_SEEDING,
    COL_COUNT
};

TorrentListWidget::TorrentListWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        tr("Name"), tr("Size"), tr("Status"),
        tr("Seeds"), tr("Peers"), tr("Progress"), tr("Seeding")
    });
    m_table->horizontalHeader()->setSectionResizeMode(COL_NAME,  QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_SIZE,  QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_STATUS,QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_SEEDS, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_PEERS, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_PROGRESS, QHeaderView::Fixed);
    m_table->setColumnWidth(COL_PROGRESS, 130);
    m_table->horizontalHeader()->setSectionResizeMode(COL_SEEDING,QHeaderView::ResizeToContents);

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setSortingEnabled(true);

    m_progressDelegate = new ProgressDelegate(this);
    m_table->setItemDelegateForColumn(COL_PROGRESS, m_progressDelegate);

    lay->addWidget(m_table);

    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &TorrentListWidget::onSelectionChanged);
}

void TorrentListWidget::setTorrents(const QList<TorrentItem> &items)
{
    m_items = items;
    applyFilter();
}

void TorrentListWidget::setFilter(const QString &text)
{
    m_filter = text;
    applyFilter();
}

int TorrentListWidget::selectedId() const
{
    auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return -1;
    return m_table->item(rows.first().row(), COL_NAME)
               ->data(Qt::UserRole).toInt();
}

void TorrentListWidget::onSelectionChanged()
{
    const int id = selectedId();
    for (const auto &t : m_filtered)
        if (t.id == id) { emit selectionChanged(t); return; }
}

void TorrentListWidget::applyFilter()
{
    m_filtered.clear();
    for (const auto &t : m_items)
        if (m_filter.isEmpty() ||
            t.name.contains(m_filter, Qt::CaseInsensitive))
            m_filtered.append(t);
    rebuildTable();
}

void TorrentListWidget::rebuildTable()
{
    const int selId = selectedId();

    m_table->setUpdatesEnabled(false);
    m_table->setSortingEnabled(false);
    m_table->setRowCount(m_filtered.size());

    for (int row = 0; row < m_filtered.size(); ++row) {
        const TorrentItem &t = m_filtered[row];

        auto *nameItem = new QTableWidgetItem(t.name);
        nameItem->setData(Qt::UserRole, t.id);
        m_table->setItem(row, COL_NAME, nameItem);

        m_table->setItem(row, COL_SIZE,
            new QTableWidgetItem(formatSize(t.sizeBytes)));

        auto *statusItem = new QTableWidgetItem(t.status);
        if (t.status == "Downloading")
            statusItem->setForeground(QColor("#7986cb"));
        else if (t.status == "Seeding")
            statusItem->setForeground(QColor("#66bb6a"));
        else if (t.status == "Paused")
            statusItem->setForeground(QColor("#ffa726"));
        else if (t.status == "Error")
            statusItem->setForeground(QColor("#ef5350"));
        m_table->setItem(row, COL_STATUS, statusItem);

        m_table->setItem(row, COL_SEEDS,
            new QTableWidgetItem(QString::number(t.seeds)));
        m_table->setItem(row, COL_PEERS,
            new QTableWidgetItem(QString::number(t.peers)));

        auto *progItem = new QTableWidgetItem();
        progItem->setData(Qt::UserRole, t.progress);
        progItem->setData(Qt::DisplayRole, QString("%1%").arg(
            qRound(t.progress * 100)));
        m_table->setItem(row, COL_PROGRESS, progItem);

        auto *seedingItem = new QTableWidgetItem(
            t.seeding ? tr("✓ Yes") : tr("No"));
        seedingItem->setForeground(t.seeding ? QColor("#66bb6a")
                                             : QColor("#6b7194"));
        m_table->setItem(row, COL_SEEDING, seedingItem);

        m_table->setRowHeight(row, 36);
    }

    m_table->setSortingEnabled(true);
    m_table->setUpdatesEnabled(true);

    if (selId != -1) {
        for (int r = 0; r < m_table->rowCount(); ++r) {
            if (m_table->item(r, COL_NAME)->data(Qt::UserRole).toInt() == selId) {
                m_table->selectRow(r);
                break;
            }
        }
    }
}

QString TorrentListWidget::formatSize(quint64 bytes)
{
    if (bytes == 0) return QStringLiteral("—");
    const double kb = 1024, mb = kb*1024, gb = mb*1024;
    if (bytes >= gb)  return QString("%1 GB").arg(bytes/gb,  0,'f',2);
    if (bytes >= mb)  return QString("%1 MB").arg(bytes/mb,  0,'f',1);
    if (bytes >= kb)  return QString("%1 KB").arg(bytes/kb,  0,'f',0);
    return QString("%1 B").arg(bytes);
}
