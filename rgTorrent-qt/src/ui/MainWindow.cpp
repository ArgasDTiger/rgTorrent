#include "MainWindow.h"
#include "TorrentListWidget.h"
#include "TorrentDetailsPanel.h"
#include "ThemeManager.h"
#include "CreateTorrentDialog.h"
#include "TorrentContentDialog.h"
#include "../backend/TorrentBackend.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSplitter>
#include <QFrame>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QTranslator>
#include <QApplication>
#include <QSettings>

MainWindow::MainWindow(TorrentBackend *backend, ThemeManager *theme, QWidget *parent)
    : QMainWindow(parent), m_backend(backend), m_theme(theme) {
    m_translator = new QTranslator(this);

    const QSettings s("rgTorrent", "rgTorrent");
    const int lang = s.value("language", 0).toInt();

    if (const QString langCode = lang == 1 ? "uk" : "en"; m_translator->load(
        QString(":/i18n/rgTorrent_%1").arg(langCode))) {
        qApp->installTranslator(m_translator);
    }

    buildUi();
    retranslateUi();

    connect(m_backend, &TorrentBackend::torrentsChanged,
            m_listWidget, &TorrentListWidget::setTorrents);
    connect(m_backend, &TorrentBackend::torrentsChanged,
            this, [this](const QList<TorrentItem> &items) {
                const int id = m_listWidget->selectedId();
                if (id == -1) return;
                for (const auto &t: items)
                    if (t.id == id) {
                        m_detailPanel->setTorrent(t);
                        return;
                    }
            });
    connect(m_backend, &TorrentBackend::errorOccurred,
            this, [this](const QString &msg) {
                statusBar()->showMessage(msg, 4000);
            });

    m_listWidget->setTorrents(m_backend->torrents());

    m_langCombo->blockSignals(true);
    m_langCombo->setCurrentIndex(lang);
    m_langCombo->blockSignals(false);

    restoreGeometry(s.value("geometry").toByteArray());
    restoreState(s.value("windowState").toByteArray());
}

void MainWindow::buildUi() {
    setMinimumSize(900, 560);
    resize(1200, 700);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLay = new QVBoxLayout(central);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);

    auto *topBar = new QFrame(central);
    topBar->setObjectName("topBar");
    topBar->setFixedHeight(56);
    buildTopBar(topBar);
    mainLay->addWidget(topBar);

    m_splitter = new QSplitter(Qt::Horizontal, central);
    m_splitter->setHandleWidth(1);

    m_listWidget = new TorrentListWidget(m_splitter);
    m_detailPanel = new TorrentDetailsPanel(m_splitter);

    m_splitter->addWidget(m_listWidget);
    m_splitter->addWidget(m_detailPanel);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({840, 320});

    mainLay->addWidget(m_splitter, 1);

    statusBar()->showMessage(tr("Ready"));

    connect(m_listWidget, &TorrentListWidget::selectionChanged,
            this, [this](const TorrentItem &t) { m_detailPanel->setTorrent(t); });

    connect(m_searchEdit, &QLineEdit::textChanged,
            m_listWidget, &TorrentListWidget::setFilter);

    connect(m_addBtn, &QPushButton::clicked, this, &MainWindow::onAddTorrent);
    connect(m_removeBtn, &QPushButton::clicked, this, &MainWindow::onRemoveTorrent);
    connect(m_createBtn, &QPushButton::clicked, this, &MainWindow::onCreateTorrent);
    connect(m_contentBtn, &QPushButton::clicked, this, &MainWindow::onShowContent);
    connect(m_themeBtn, &QPushButton::clicked, this, &MainWindow::onThemeToggled);

    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLanguageChanged);

    connect(m_theme, &ThemeManager::themeChanged, this, [this](ThemeManager::Theme t) {
        m_themeBtn->setText(t == ThemeManager::Dark ? "☀" : "🌑");
    });
    m_themeBtn->setText("☀");
}

void MainWindow::buildTopBar(QWidget *bar) {
    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(16, 0, 16, 0);
    lay->setSpacing(10);

    m_titleLabel = new QLabel("rgTorrent", bar);
    m_titleLabel->setObjectName("titleLabel");
    lay->addWidget(m_titleLabel);

    lay->addSpacing(20);

    m_searchEdit = new QLineEdit(bar);
    m_searchEdit->setPlaceholderText(tr("Filter torrents…"));
    m_searchEdit->setMinimumWidth(200);
    m_searchEdit->setMaximumWidth(320);
    lay->addWidget(m_searchEdit);

    lay->addStretch();

    m_addBtn = new QPushButton(tr("＋ Add"), bar);
    m_addBtn->setObjectName("accentBtn");

    m_removeBtn = new QPushButton(tr("✕ Remove"), bar);
    m_removeBtn->setObjectName("dangerBtn");

    m_createBtn = new QPushButton(tr("⚙ Create"), bar);

    m_contentBtn = new QPushButton(tr("🔍 Contents"), bar);

    for (auto *b: {m_addBtn, m_removeBtn, m_createBtn, m_contentBtn}) {
        b->setFixedHeight(32);
        lay->addWidget(b);
    }

    lay->addSpacing(10);

    m_langCombo = new QComboBox(bar);
    m_langCombo->addItem("English", "en");
    m_langCombo->addItem("Українська", "uk");
    m_langCombo->setFixedHeight(32);
    lay->addWidget(m_langCombo);

    m_themeBtn = new QPushButton("☀", bar);
    m_themeBtn->setObjectName("iconBtn");
    m_themeBtn->setFixedSize(32, 32);
    m_themeBtn->setFont(QFont("", 16));
    lay->addWidget(m_themeBtn);
}

void MainWindow::retranslateUi() {
    setWindowTitle(tr("rgTorrent"));
    m_searchEdit->setPlaceholderText(tr("Filter torrents…"));
    m_addBtn->setText(tr("＋ Add"));
    m_removeBtn->setText(tr("✕ Remove"));
    m_createBtn->setText(tr("⚙ Create"));
    m_contentBtn->setText(tr("🔍 Contents"));
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::onAddTorrent() {
    const QString torrentFile = QFileDialog::getOpenFileName(
        this, tr("Open torrent file"), QString(),
        tr("Torrent files (*.torrent);;All files (*)"));
    if (torrentFile.isEmpty()) return;

    const QString saveDir = QFileDialog::getExistingDirectory(
        this, tr("Select download directory"));
    if (saveDir.isEmpty()) return;

    m_backend->addTorrent(torrentFile, saveDir);
    statusBar()->showMessage(tr("Torrent added: %1").arg(torrentFile), 3000);
}

void MainWindow::onRemoveTorrent() {
    const int id = m_listWidget->selectedId();
    if (id == -1) {
        statusBar()->showMessage(tr("No torrent selected."), 2000);
        return;
    }
    const auto btn = QMessageBox::question(
        this, tr("Remove torrent"),
        tr("Remove the selected torrent from the list?"),
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;

    m_backend->removeTorrent(id);
    m_detailPanel->clear();
    statusBar()->showMessage(tr("Torrent removed."), 2000);
}

void MainWindow::onCreateTorrent() {
    CreateTorrentDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    if (dlg.sourceDir().isEmpty() || dlg.outputPath().isEmpty()) return;

    m_backend->createTorrent(dlg.sourceDir(), dlg.outputPath(), dlg.trackerUrl(), dlg.pieceLength());

    statusBar()->showMessage(tr("Torrent created: %1").arg(dlg.outputPath()), 3000);
}

void MainWindow::onShowContent() {
    const QString torrentFile = QFileDialog::getOpenFileName(
        this, tr("Open torrent file"), QString(),
        tr("Torrent files (*.torrent);;All files (*)"));
    if (torrentFile.isEmpty()) return;
    const QString contents = TorrentBackend::torrentContents(torrentFile);
    TorrentContentDialog dlg(torrentFile, contents, this);
    dlg.exec();
}

void MainWindow::onThemeToggled() const {
    m_theme->toggleTheme();
    QSettings("rgTorrent", "rgTorrent").setValue(
        "theme", m_theme->currentTheme() == ThemeManager::Dark ? "dark" : "light");
}

void MainWindow::onLanguageChanged(const int index) {
    const QString lang = m_langCombo->itemData(index).toString();
    QSettings("rgTorrent", "rgTorrent").setValue("language", index);

    qApp->removeTranslator(m_translator);
    if (m_translator->load(QString(":/i18n/rgTorrent_%1").arg(lang))) {
        qApp->installTranslator(m_translator);
    }

    retranslateUi();
    m_listWidget->retranslateUi();
    m_detailPanel->retranslateUi();
}

void MainWindow::closeEvent(QCloseEvent *ev) {
    QSettings s("rgTorrent", "rgTorrent");
    s.setValue("geometry", saveGeometry());
    s.setValue("windowState", saveState());
    m_backend->saveSession();
    QMainWindow::closeEvent(ev);
}
