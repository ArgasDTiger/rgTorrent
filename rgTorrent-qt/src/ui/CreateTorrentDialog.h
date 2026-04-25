#pragma once
#include <QDialog>

class QLineEdit;
class QPushButton;

class CreateTorrentDialog final : public QDialog {
    Q_OBJECT
public:
    explicit CreateTorrentDialog(QWidget *parent = nullptr);

    QString sourceDir()   const;
    QString outputPath()  const;
    QString trackerUrl()  const;

private:
    QLineEdit *m_sourceDirEdit;
    QLineEdit *m_outputPathEdit;
    QLineEdit *m_trackerEdit;
};
