#pragma once
#include <QObject>
#include <QString>

class QApplication;

class ThemeManager final : public QObject {
    Q_OBJECT
public:
    enum Theme { Dark, Light };

    explicit ThemeManager(QApplication *app, QObject *parent = nullptr);

    [[nodiscard]] Theme currentTheme() const { return m_theme; }

public slots:
    void setTheme(Theme t);
    void toggleTheme();

signals:
    void themeChanged(Theme t);

private:
    QApplication *m_app;
    Theme         m_theme = Dark;

    static QString darkStyleSheet();

    static QString lightStyleSheet();
};
