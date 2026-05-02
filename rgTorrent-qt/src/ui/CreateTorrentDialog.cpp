#include "CreateTorrentDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QHBoxLayout>

CreateTorrentDialog::CreateTorrentDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Create Torrent"));
    setMinimumWidth(480);

    auto *vlay = new QVBoxLayout(this);
    vlay->setSpacing(14);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);

    m_sourceDirEdit = new QLineEdit(this);
    m_sourceDirEdit->setPlaceholderText(tr("Select source folder…"));
    auto *browseSrc = new QPushButton("…", this);
    browseSrc->setFixedWidth(32);
    auto *srcRow = new QHBoxLayout;
    srcRow->addWidget(m_sourceDirEdit);
    srcRow->addWidget(browseSrc);
    form->addRow(tr("Source folder:"), srcRow);

    m_outputPathEdit = new QLineEdit(this);
    m_outputPathEdit->setPlaceholderText(tr("Save .torrent as..."));
    auto *browseOut = new QPushButton("…", this);
    browseOut->setFixedWidth(32);
    auto *outRow = new QHBoxLayout;
    outRow->addWidget(m_outputPathEdit);
    outRow->addWidget(browseOut);
    form->addRow(tr("Output file:"), outRow);

    m_trackerEdit = new QLineEdit(this);
    m_trackerEdit->setPlaceholderText("udp://tracker.example.com:6969/announce");
    form->addRow(tr("Tracker URL:"), m_trackerEdit);

    m_pieceSizeCombo = new QComboBox(this);
    m_pieceSizeCombo->addItem("256 KB", 262144);
    m_pieceSizeCombo->addItem("512 KB", 524288);
    m_pieceSizeCombo->addItem("1 MB", 1048576);
    m_pieceSizeCombo->addItem("2 MB", 2097152);
    m_pieceSizeCombo->addItem("4 MB", 4194304);
    m_pieceSizeCombo->setCurrentIndex(1);
    form->addRow(tr("Piece Size:"), m_pieceSizeCombo);

    vlay->addLayout(form);

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    vlay->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(browseSrc, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(
            this, tr("Select source folder"), m_sourceDirEdit->text());
        if (!d.isEmpty()) m_sourceDirEdit->setText(d);
    });

    connect(browseOut, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(
            this, tr("Save torrent file"), m_outputPathEdit->text(),
            tr("Torrent files (*.torrent)"));
        if (!f.isEmpty()) m_outputPathEdit->setText(f);
    });
}

QString CreateTorrentDialog::sourceDir()  const { return m_sourceDirEdit->text(); }
QString CreateTorrentDialog::outputPath() const { return m_outputPathEdit->text(); }
QString CreateTorrentDialog::trackerUrl() const { return m_trackerEdit->text(); }
int CreateTorrentDialog::pieceLength() const {
    return m_pieceSizeCombo->currentData().toInt();
}
