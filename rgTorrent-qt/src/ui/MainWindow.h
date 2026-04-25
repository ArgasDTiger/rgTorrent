#pragma once
#include <QMainWindow>

class TorrentBackend;
class TorrentListWidget;
class TorrentDetailsPanel;
class ThemeManager;
class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QTranslator;
class QSplitter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(TorrentBackend *backend,
                        ThemeManager   *theme,
                        QWidget        *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *ev) override;

private slots:
    void onAddTorrent();
    void onRemoveTorrent();
    void onCreateTorrent();
    void onShowContent();
    void onThemeToggled() const;
    void onLanguageChanged(int index);

private:
    void buildUi();
    void buildTopBar(QWidget *bar);
    void retranslateUi();

    TorrentBackend    *m_backend;
    ThemeManager      *m_theme;
    TorrentListWidget *m_listWidget;
    TorrentDetailsPanel *m_detailPanel;
    QLineEdit         *m_searchEdit;
    QPushButton       *m_addBtn;
    QPushButton       *m_removeBtn;
    QPushButton       *m_createBtn;
    QPushButton       *m_contentBtn;
    QPushButton       *m_themeBtn;
    QComboBox         *m_langCombo;
    QLabel            *m_titleLabel;
    QSplitter         *m_splitter;
    QTranslator       *m_translator;
};