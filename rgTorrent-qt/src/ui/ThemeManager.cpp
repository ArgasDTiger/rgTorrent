#include "ThemeManager.h"
#include <QApplication>
#include <QPalette>

ThemeManager::ThemeManager(QApplication *app, QObject *parent)
    : QObject(parent), m_app(app)
{
    setTheme(Dark);
}

void ThemeManager::setTheme(Theme t)
{
    m_theme = t;
    m_app->setStyleSheet(t == Dark ? darkStyleSheet() : lightStyleSheet());
    emit themeChanged(t);
}

void ThemeManager::toggleTheme()
{
    setTheme(m_theme == Dark ? Light : Dark);
}

QString ThemeManager::darkStyleSheet() {
    return QStringLiteral(R"(
QWidget {
    background-color: #12131a;
    color: #e2e4ed;
    font-family: "JetBrains Mono", "Fira Code", "Courier New", monospace;
    font-size: 13px;
}

QMainWindow, QDialog {
    background-color: #12131a;
}

QFrame#sidePanel {
    background-color: #0e0f15;
    border-right: 1px solid #1e2030;
}

QFrame#topBar {
    background-color: #0e0f15;
    border-bottom: 1px solid #1e2030;
}

QTableWidget, QTreeWidget {
    background-color: #12131a;
    alternate-background-color: #161822;
    gridline-color: #1e2030;
    border: none;
    selection-background-color: #2a2d44;
    selection-color: #c8ceff;
    outline: none;
}

QTableWidget::item, QTreeWidget::item {
    padding: 6px 8px;
    border: none;
}

QTableWidget::item:hover, QTreeWidget::item:hover {
    background-color: #1e2030;
}

QHeaderView::section {
    background-color: #0e0f15;
    color: #6b7194;
    padding: 6px 8px;
    border: none;
    border-right: 1px solid #1e2030;
    border-bottom: 1px solid #1e2030;
    font-size: 11px;
    letter-spacing: 0.06em;
    text-transform: uppercase;
}

QProgressBar {
    background-color: #1e2030;
    border: none;
    border-radius: 3px;
    height: 6px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #5c6bc0, stop:1 #7986cb);
    border-radius: 3px;
}

QPushButton {
    background-color: #1e2030;
    color: #c8ceff;
    border: 1px solid #2a2d44;
    border-radius: 5px;
    padding: 6px 14px;
    font-size: 12px;
}
QPushButton:hover {
    background-color: #2a2d44;
    border-color: #5c6bc0;
}
QPushButton:pressed {
    background-color: #3a3f62;
}
QPushButton#accentBtn {
    background-color: #5c6bc0;
    color: #ffffff;
    border: none;
}
QPushButton#accentBtn:hover {
    background-color: #7986cb;
}
QPushButton#dangerBtn {
    background-color: #1e2030;
    color: #ef5350;
    border: 1px solid #3b1e1e;
}
QPushButton#dangerBtn:hover {
    background-color: #3b1e1e;
    border-color: #ef5350;
}
QPushButton#iconBtn {
    background: transparent;
    border: none;
    padding: 4px;
    color: #6b7194;
}
QPushButton#iconBtn:hover { color: #c8ceff; }

QLineEdit {
    background-color: #1a1b26;
    color: #e2e4ed;
    border: 1px solid #2a2d44;
    border-radius: 5px;
    padding: 5px 10px;
    selection-background-color: #5c6bc0;
}
QLineEdit:focus {
    border-color: #5c6bc0;
}
QLineEdit[placeholderText] { color: #4a4f72; }

QLabel#titleLabel {
    font-size: 18px;
    font-weight: bold;
    color: #c8ceff;
    letter-spacing: 0.04em;
}
QLabel#sectionLabel {
    color: #6b7194;
    font-size: 11px;
    letter-spacing: 0.08em;
}
QLabel#detailKey {
    color: #6b7194;
    font-size: 12px;
}
QLabel#detailValue {
    color: #e2e4ed;
    font-size: 12px;
}

QFrame#detailPanel {
    background-color: #0e0f15;
    border-left: 1px solid #1e2030;
}

QScrollBar:vertical {
    background: #12131a;
    width: 8px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #2a2d44;
    border-radius: 4px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: #5c6bc0; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }

QScrollBar:horizontal {
    background: #12131a;
    height: 8px;
}
QScrollBar::handle:horizontal {
    background: #2a2d44;
    border-radius: 4px;
    min-width: 30px;
}
QScrollBar::handle:horizontal:hover { background: #5c6bc0; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

QComboBox {
    background-color: #1e2030;
    color: #c8ceff;
    border: 1px solid #2a2d44;
    border-radius: 5px;
    padding: 4px 10px;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background-color: #1a1b26;
    border: 1px solid #2a2d44;
    selection-background-color: #2a2d44;
}

QSplitter::handle { background-color: #1e2030; }

QStatusBar {
    background-color: #0e0f15;
    color: #4a4f72;
    border-top: 1px solid #1e2030;
    font-size: 11px;
}

QToolTip {
    background-color: #1a1b26;
    color: #c8ceff;
    border: 1px solid #2a2d44;
    padding: 4px 8px;
    border-radius: 4px;
}

QTextBrowser {
    background-color: #0e0f15;
    color: #c8ceff;
    border: none;
    font-family: "JetBrains Mono", monospace;
    font-size: 12px;
}
    )");
}

QString ThemeManager::lightStyleSheet() {
    return QStringLiteral(R"(
QWidget {
    background-color: #f4f5f9;
    color: #1a1b2e;
    font-family: "JetBrains Mono", "Fira Code", "Courier New", monospace;
    font-size: 13px;
}

QMainWindow, QDialog {
    background-color: #f4f5f9;
}

QFrame#sidePanel {
    background-color: #ffffff;
    border-right: 1px solid #dde0f0;
}

QFrame#topBar {
    background-color: #ffffff;
    border-bottom: 1px solid #dde0f0;
}

QTableWidget, QTreeWidget {
    background-color: #ffffff;
    alternate-background-color: #f8f9fd;
    gridline-color: #dde0f0;
    border: none;
    selection-background-color: #e8eaff;
    selection-color: #3949ab;
    outline: none;
}

QTableWidget::item, QTreeWidget::item {
    padding: 6px 8px;
    border: none;
}

QTableWidget::item:hover, QTreeWidget::item:hover {
    background-color: #eef0ff;
}

QHeaderView::section {
    background-color: #f0f1fa;
    color: #7986cb;
    padding: 6px 8px;
    border: none;
    border-right: 1px solid #dde0f0;
    border-bottom: 1px solid #dde0f0;
    font-size: 11px;
    letter-spacing: 0.06em;
}

QProgressBar {
    background-color: #dde0f0;
    border: none;
    border-radius: 3px;
    height: 6px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
        stop:0 #3949ab, stop:1 #5c6bc0);
    border-radius: 3px;
}

QPushButton {
    background-color: #f0f1fa;
    color: #3949ab;
    border: 1px solid #dde0f0;
    border-radius: 5px;
    padding: 6px 14px;
    font-size: 12px;
}
QPushButton:hover {
    background-color: #e8eaff;
    border-color: #5c6bc0;
}
QPushButton:pressed {
    background-color: #c5cae9;
}
QPushButton#accentBtn {
    background-color: #3949ab;
    color: #ffffff;
    border: none;
}
QPushButton#accentBtn:hover {
    background-color: #5c6bc0;
}
QPushButton#dangerBtn {
    background-color: #f0f1fa;
    color: #e53935;
    border: 1px solid #ffd0d0;
}
QPushButton#dangerBtn:hover {
    background-color: #ffebee;
    border-color: #e53935;
}
QPushButton#iconBtn {
    background: transparent;
    border: none;
    padding: 4px;
    color: #9fa8da;
}
QPushButton#iconBtn:hover { color: #3949ab; }

QLineEdit {
    background-color: #ffffff;
    color: #1a1b2e;
    border: 1px solid #dde0f0;
    border-radius: 5px;
    padding: 5px 10px;
    selection-background-color: #c5cae9;
}
QLineEdit:focus { border-color: #3949ab; }

QLabel#titleLabel {
    font-size: 18px;
    font-weight: bold;
    color: #3949ab;
}
QLabel#sectionLabel {
    color: #9fa8da;
    font-size: 11px;
    letter-spacing: 0.08em;
}
QLabel#detailKey { color: #9fa8da; font-size: 12px; }
QLabel#detailValue { color: #1a1b2e; font-size: 12px; }

QFrame#detailPanel {
    background-color: #ffffff;
    border-left: 1px solid #dde0f0;
}

QScrollBar:vertical {
    background: #f4f5f9;
    width: 8px; margin: 0;
}
QScrollBar::handle:vertical {
    background: #c5cae9;
    border-radius: 4px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: #7986cb; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }

QScrollBar:horizontal {
    background: #f4f5f9;
    height: 8px;
}
QScrollBar::handle:horizontal {
    background: #c5cae9;
    border-radius: 4px;
    min-width: 30px;
}
QScrollBar::handle:horizontal:hover { background: #7986cb; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

QComboBox {
    background-color: #f0f1fa;
    color: #3949ab;
    border: 1px solid #dde0f0;
    border-radius: 5px;
    padding: 4px 10px;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background-color: #ffffff;
    border: 1px solid #dde0f0;
    selection-background-color: #e8eaff;
}

QSplitter::handle { background-color: #dde0f0; }

QStatusBar {
    background-color: #ffffff;
    color: #9fa8da;
    border-top: 1px solid #dde0f0;
    font-size: 11px;
}

QToolTip {
    background-color: #ffffff;
    color: #3949ab;
    border: 1px solid #dde0f0;
    padding: 4px 8px;
    border-radius: 4px;
}

QTextBrowser {
    background-color: #f8f9fd;
    color: #3949ab;
    border: none;
    font-family: "JetBrains Mono", monospace;
    font-size: 12px;
}
    )");
}
