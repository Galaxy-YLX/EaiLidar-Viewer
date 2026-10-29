#include "ui/scan_view.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>

namespace eai {

namespace {

constexpr double kPi = 3.14159265358979323846;

// 新点颜色（绿）
const QColor kPointColor(0, 220, 120);

// 按量程选择网格/标尺步进（mm），使屏幕上始终有 5~10 条刻度，
// 小量程自动变细（如 1 m 量程 → 0.1 m 步进）。
double niceStep(double range_mm) {
    const double target = range_mm / 8.0;
    const double candidates[] = {100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0};
    double best = candidates[0];
    for (double s : candidates) {
        // 取最接近目标的档位，避免大步进导致刻度过稀
        if (std::abs(s - target) < std::abs(best - target)) best = s;
    }
    return best;
}

} // namespace

ScanView::ScanView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 400);
}

void ScanView::setScan(const std::shared_ptr<const Scan>& scan) {
    scan_ = scan;
    update();
}

void ScanView::setPointRadius(double r) {
    point_radius_ = std::max(0.1, r);
    update();
}

void ScanView::setRangeMm(double range_mm) {
    // 雷达最大量程 20 m；下限 0.2 m 防缩放过小
    range_mm_ = std::clamp(range_mm, 200.0, 20000.0);
    update();
}

void ScanView::setMirror(int mode) {
    mirror_ = mode & 3; // 0/1/2/3
    update();
}

void ScanView::setRotationDeg(double deg) {
    rotation_deg_ = deg;
    update();
}

void ScanView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0));

    const double cx = width() / 2.0;
    const double cy = height() / 2.0;
    const double scale = std::min(width(), height()) / (2.0 * range_mm_);

    // 世界坐标 (mm) → 屏幕坐标（雷达居中，y 轴向上为正，屏幕 y 向下）。
    // 先镜像、再旋转（数学正方向 = 屏幕顺时针），用于适配雷达安装方位。
    const double rot = rotation_deg_ * kPi / 180.0;
    const double cosR = std::cos(rot);
    const double sinR = std::sin(rot);
    auto toScreen = [&](double x_mm, double y_mm) {
        double wx = x_mm;
        double wy = y_mm;
        if (mirror_ & 1) wx = -wx; // 左右镜像
        if (mirror_ & 2) wy = -wy; // 上下镜像
        const double rx = wx * cosR - wy * sinR;
        const double ry = wx * sinR + wy * cosR;
        return QPointF(cx + rx * scale, cy - ry * scale);
    };

    // 距离网格（步进随量程自适应）
    const double step = niceStep(range_mm_);
    p.setPen(QPen(QColor(34, 42, 36), 1));
    for (double d = step; d < range_mm_; d += step) {
        const double r = d * scale;
        p.drawEllipse(QPointF(cx, cy), r, r);
    }
    p.drawLine(QPointF(cx - range_mm_ * scale, cy), QPointF(cx + range_mm_ * scale, cy));
    p.drawLine(QPointF(cx, cy - range_mm_ * scale), QPointF(cx, cy + range_mm_ * scale));

    // 距离标尺：X 轴正半轴，步进与网格一致 + 距离标注
    p.setPen(QPen(QColor(96, 110, 100), 1));
    const QFont oldFont = p.font();
    QFont rulerFont = oldFont;
    rulerFont.setPointSizeF(8.0);
    p.setFont(rulerFont);
    for (double d = step; d < range_mm_; d += step) {
        const double x = cx + d * scale;
        p.drawLine(QPointF(x, cy - 4), QPointF(x, cy + 4));
        const double meters = d / 1000.0;
        const QString label = std::abs(meters - std::round(meters)) < 1e-9
                                  ? QString::number(static_cast<int>(std::lround(meters)))
                                  : QString::number(meters, 'f', 1);
        p.drawText(QRectF(x - 24, cy + 6, 48, 12), Qt::AlignHCenter | Qt::AlignTop,
                   label + tr("m"));
    }
    p.setFont(oldFont);

    // 整圈替换，直接画最新一圈
    if (!scan_ || scan_->points.empty()) {
        p.setPen(QColor(60, 92, 68));
        p.drawText(rect(), Qt::AlignCenter, tr("No data"));
        return;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(kPointColor);
    for (const auto& pt : scan_->points) {
        if (!pt.valid) continue;
        p.drawEllipse(toScreen(pt.x_mm, pt.y_mm), point_radius_, point_radius_);
    }
}

} // namespace eai
