#pragma once
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPolygonF>
#include <QWidget>

namespace cc::app {

/// Piece picture of the dialog (our own drawing of orig_artic_08): a grey I-beam with the axes X (red), Y (green) and Z (blue).
class PiecePicture : public QWidget {
public:
    explicit PiecePicture(QWidget* parent) : QWidget(parent) { setFixedSize(110, 84); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF o(46, 60);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(110, 110, 110));
        p.drawPolygon(QPolygonF({QPointF(50, 22), QPointF(86, 14), QPointF(90, 22), QPointF(56, 30)}));
        p.setBrush(QColor(80, 80, 80));
        p.drawPolygon(QPolygonF({QPointF(56, 30), QPointF(90, 22), QPointF(92, 46), QPointF(58, 54)}));
        p.setBrush(QColor(140, 140, 140));
        p.drawPolygon(QPolygonF({QPointF(50, 46), QPointF(58, 54), QPointF(92, 46), QPointF(84, 40)}));
        const struct { QPointF to; QColor colour; const char* name; } axes[3] = {
            {QPointF(18, 74), QColor(220, 0, 0), "X"}, {QPointF(80, 74), QColor(0, 170, 0), "Y"}, {QPointF(46, 6), QColor(0, 0, 230), "Z"}};
        for (const auto& axis : axes) {
            p.setPen(QPen(axis.colour, 1.6));
            p.drawLine(o, axis.to);
            QFont f = font();
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRectF(axis.to.x() - 8, axis.to.y() - 9, 16, 14), Qt::AlignCenter, QString::fromLatin1(axis.name));
        }
    }
};

}  // namespace cc::app
