#pragma once
#include <QDialog>
#include <QComboBox>

class QLineEdit;
class QPushButton;

class CreateTorrentDialog final : public QDialog {
    Q_OBJECT
public:
    explicit CreateTorrentDialog(QWidget *parent = nullptr);

    [[nodiscard]] QString sourceDir() const;
    [[nodiscard]] QString outputPath() const;
    [[nodiscard]] QString trackerUrl() const;
    [[nodiscard]] int pieceLength() const;

private:
    QLineEdit *m_sourceDirEdit;
    QLineEdit *m_outputPathEdit;
    QLineEdit *m_trackerEdit;
    QComboBox *m_pieceSizeCombo;
};
