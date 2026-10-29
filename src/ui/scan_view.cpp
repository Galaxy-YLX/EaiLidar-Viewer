#include "ui/scan_view.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>

namespace eai {

ScanView::ScanView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 400);
}

void ScanView::setScan(const std::shared_ptr<const Scan>& scan) {
    scan_ = scan;
    update();
}

void ScanView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(16, 16, 20));

    const double cx = width() / 2.0;
    const double cy = height() / 2.0;
    const double scale = std::min(width(), height()) / (2.0 * range_mm_);

    // 世界坐标 (mm) → 屏幕坐标（雷达居中，y 轴向上为正，屏幕 y 向下）
    auto toScreen = [&](double x_mm, double y_mm) {
        return QPointF(cx + x_mm * scale, cy - y_mm * scale);
    };

    // 距离网格（每 1 m 一圈）
    p.setPen(QPen(QColor(48, 48, 56), 1));
    for (double d = 1000.0; d <= range_mm_; d += 1000.0) {
        const double r = d * scale;
        p.drawEllipse(QPointF(cx, cy), r, r);
    }
    p.drawLine(QPointF(cx - range_mm_ * scale, cy), QPointF(cx + range_mm_ * scale, cy));
    p.drawLine(QPointF(cx, cy - range_mm_ * scale), QPointF(cx, cy + range_mm_ * scale));

    if (!scan_ || scan_->points.empty()) {
        p.setPen(QColor(120, 120, 128));
        p.drawText(rect(), Qt::AlignCenter, tr("No data"));
        return;
    }

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 220, 120));
    const double r_pt = 2.0;
    for (const auto& pt : scan_->points) {
        if (!pt.valid) continue;
        const QPointF s = toScreen(pt.x_mm, pt.y_mm);
        p.drawEllipse(s, r_pt, r_pt);
    }
}

} // namespace eai
