#pragma once

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QString>
#include <QColor>

namespace IconUtils {
    inline QIcon colorize(const QString &path, const QColor &color) {
        QPixmap pm = QIcon(path).pixmap(24, 24);
        QPainter p(&pm);

        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pm.rect(), color);
        p.end();

        return QIcon(pm);
    }

}