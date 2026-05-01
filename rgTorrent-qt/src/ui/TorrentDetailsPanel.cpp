#include "TorrentDetailsPanel.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QFrame>
#include <QScrollArea>

TorrentDetailsPanel::TorrentDetailsPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("detailPanel");
    setMinimumWidth(240);

    auto *outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0,0,0,0);

    auto *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);

    auto *content = new QWidget;
    auto *vlay = new QVBoxLayout(content);
    vlay->setContentsMargins(16, 16, 16, 16);
    vlay->setSpacing(12);

    auto *title = new QLabel(tr("TORRENT DETAILS"), content);
    title->setObjectName("sectionLabel");
    vlay->addWidget(title);

    m_card = new QFrame(content);
    m_card->setFrameShape(QFrame::StyledPanel);
    auto *grid = new QGridLayout(m_card);
    grid->setContentsMargins(14,14,14,14);
    grid->setVerticalSpacing(10);
    grid->setHorizontalSpacing(16);
    grid->setColumnStretch(1, 1);

    int row = 0;
    addRow(m_card, row++, tr("Name"),     &m_lName);
    addRow(m_card, row++, tr("Size"),     &m_lSize);
    addRow(m_card, row++, tr("Status"),   &m_lStatus);
    addRow(m_card, row++, tr("Seeds"),    &m_lSeeds);
    addRow(m_card, row++, tr("Peers"),    &m_lPeers);
    addRow(m_card, row++, tr("Seeding"),  &m_lSeeding);
    addRow(m_card, row++, tr("File"),     &m_lPath);

    auto *progKey = new QLabel(tr("Progress"), m_card);
    progKey->setObjectName("detailKey");
    m_progressBar = new QProgressBar(m_card);
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(14);
    m_lProgress = new QLabel("0%", m_card);
    m_lProgress->setObjectName("detailValue");

    grid->addWidget(progKey, row, 0, Qt::AlignTop);
    auto *progContainer = new QWidget(m_card);
    auto *progLay = new QVBoxLayout(progContainer);
    progLay->setContentsMargins(0,0,0,0);
    progLay->setSpacing(4);
    progLay->addWidget(m_progressBar);
    progLay->addWidget(m_lProgress);
    grid->addWidget(progContainer, row, 1);

    vlay->addWidget(m_card);
    vlay->addStretch();

    scroll->setWidget(content);
    outerLay->addWidget(scroll);

    clear();
}

void TorrentDetailsPanel::addRow(QWidget *parent, const int row,
                                  const QString &key, QLabel **valueOut)
{
    auto *grid = qobject_cast<QGridLayout*>(parent->layout());
    auto *kLbl = new QLabel(key, parent);

    kLbl->setObjectName(QString("key_%1").arg(row));

    *valueOut = new QLabel(QStringLiteral("—"), parent);
    (*valueOut)->setObjectName("detailValue");
    (*valueOut)->setWordWrap(true);
    grid->addWidget(kLbl,       row, 0, Qt::AlignTop);
    grid->addWidget(*valueOut,  row, 1);
}

void TorrentDetailsPanel::setTorrent(const TorrentItem &item) const {
    m_lName->setText(item.name.isEmpty() ? QStringLiteral("—") : item.name);

    m_lSize->setText(item.sizeBytes
        ? tr("%1 bytes").arg(item.sizeBytes)
        : QStringLiteral("—"));

    m_lStatus->setText(item.status);
    m_lSeeds->setText(QString::number(item.seeds));
    m_lPeers->setText(QString::number(item.peers));
    m_lSeeding->setText(item.seeding ? tr("Yes") : tr("No"));
    m_lPath->setText(item.torrentPath.isEmpty()
        ? QStringLiteral("—") : item.torrentPath);

    const int pct = qRound(item.progress * 100);
    m_progressBar->setValue(pct);
    m_lProgress->setText(QString("%1%").arg(pct));
}

void TorrentDetailsPanel::clear()
{
    for (auto *l : {m_lName, m_lSize, m_lStatus, m_lSeeds,
                     m_lPeers, m_lSeeding, m_lPath})
        l->setText(QStringLiteral("—"));
    m_progressBar->setValue(0);
    m_lProgress->setText("0%");
}

void TorrentDetailsPanel::retranslateUi() const {
    if (const auto title = findChild<QLabel*>("sectionLabel")) title->setText(tr("TORRENT DETAILS"));
    if (const auto k0 = findChild<QLabel*>("key_0")) k0->setText(tr("Name"));
    if (const auto k1 = findChild<QLabel*>("key_1")) k1->setText(tr("Size"));
    if (const auto k2 = findChild<QLabel*>("key_2")) k2->setText(tr("Status"));
    if (const auto k3 = findChild<QLabel*>("key_3")) k3->setText(tr("Seeds"));
    if (const auto k4 = findChild<QLabel*>("key_4")) k4->setText(tr("Peers"));
    if (const auto k5 = findChild<QLabel*>("key_5")) k5->setText(tr("Seeding"));
    if (const auto k6 = findChild<QLabel*>("key_6")) k6->setText(tr("File"));
    if (const auto kp = findChild<QLabel*>("detailKey")) kp->setText(tr("Progress"));
}