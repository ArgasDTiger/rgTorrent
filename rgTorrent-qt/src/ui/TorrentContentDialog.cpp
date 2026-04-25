#include "TorrentContentDialog.h"
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QLabel>
#include <QFileInfo>

TorrentContentDialog::TorrentContentDialog(const QString &torrentPath,
                                             const QString &contents,
                                             QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Torrent Contents – %1")
        .arg(QFileInfo(torrentPath).fileName()));
    resize(600, 420);

    auto *vlay = new QVBoxLayout(this);
    vlay->setSpacing(10);

    auto *pathLabel = new QLabel(torrentPath, this);
    pathLabel->setObjectName("detailKey");
    pathLabel->setWordWrap(true);
    vlay->addWidget(pathLabel);

    auto *browser = new QTextBrowser(this);
    browser->setPlainText(contents);
    vlay->addWidget(browser, 1);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    vlay->addWidget(btns);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
