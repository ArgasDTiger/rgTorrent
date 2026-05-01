#include <QApplication>
#include <QSettings>
#include "ui/MainWindow.h"
#include "ui/ThemeManager.h"
#include "backend/TorrentBackend.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("rgTorrent");
    app.setOrganizationName("rgTorrent");
    app.setApplicationVersion("1.0");

    auto *theme = new ThemeManager(&app);
    auto *backend = new TorrentBackend;

    backend->loadSession();

    const QSettings s("rgTorrent", "rgTorrent");
    const QString savedTheme = s.value("theme", "dark").toString();
    theme->setTheme(savedTheme == "light"
        ? ThemeManager::Light : ThemeManager::Dark);

    MainWindow w(backend, theme);
    w.show();

    const int rc = app.exec();

    delete backend;
    delete theme;
    return rc;
}
