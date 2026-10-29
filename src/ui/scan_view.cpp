#include "ui/scan_view.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>

namespace eai {

namespace {

// 尾迹淡出重绘间隔：约 30 fps，保证旧点平滑变淡。
constexpr int kTailRepaintMs = 33;

// 新点颜色（绿）；旧点按年龄降低透明度，叠在深色背景上自然变暗。
const QColor kPointColor(0, 220, 120);

// 扫描频率未知（起始帧未到）时，按 7 Hz 假设一圈周期，仅影响尾迹时间展开精度。
constexpr double kDefaultPeriodMs = 1000.0 / 7.0;

} // namespace

ScanView::ScanView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 400);
    clock_.start();
    tail_timer_.setInterval(kTailRepaintMs);
    connect(&tail_timer_, &QTimer::timeout, this, [this] {
        pruneExpired(clock_.msecsSinceReference());
        update();
        if (points_.empty()) {
            tail_timer_.stop(); // 尾迹完全消失后停止空转
        }
    });
}

void ScanView::setScan(const std::shared_ptr<const Scan>& scan) {
    latest_ = scan;
    if (scan == nullptr) {
        points_.clear();
        tail_timer_.stop();
        update();
        return;
    }

    if (retain_ms_ == 0) {
        points_.clear(); // 不保留尾迹：整圈替换
        update();
        return;
    }

    appendScan(scan);
    pruneExpired(clock_.msecsSinceReference());
    if (!tail_timer_.isActive()) {
        tail_timer_.start(); // 需要持续重绘以淡出旧点
    }
    update();
}

void ScanView::setPointRadius(double r) {
    point_radius_ = std::max(0.5, r);
    update();
}

void ScanView::setRetainMs(int ms) {
    const int prev = retain_ms_;
    retain_ms_ = std::max(0, ms);
    if (retain_ms_ == 0) {
        tail_timer_.stop();
        points_.clear(); // 回到只显示最新一圈
    } else if (prev == 0 && latest_) {
        appendScan(latest_); // 刚开启尾迹：把当前一圈纳入渐隐
        tail_timer_.start();
    }
    update();
}

// 一圈的点按接收顺序排列（首点最早、末点最新）。硬件逐帧送达，一圈约 143 ms；
// 这里把整圈的点在 [now - 周期, now] 内按序号线性展开时间戳，
// 使每个点的年龄近似其真实到达时刻，尾迹渐隐平滑到点级别。
void ScanView::appendScan(const std::shared_ptr<const Scan>& scan) {
    if (!scan || scan->points.empty()) return;

    const qint64 now = clock_.msecsSinceReference();
    const double period = scan->scan_freq_hz > 0.01
                              ? 1000.0 / scan->scan_freq_hz
                              : kDefaultPeriodMs;
    const std::size_t n = scan->points.size();
    points_.reserve(points_.size() + n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& pt = scan->points[i];
        if (!pt.valid) continue;
        // i=0 是一圈前到达，i=n-1 是刚到达
        const double age = n > 1 ? period * static_cast<double>(n - 1 - i) /
                                       static_cast<double>(n - 1)
                                 : 0.0;
        points_.push_back({pt.x_mm, pt.y_mm, now - static_cast<qint64>(std::lround(age))});
    }
}

void ScanView::pruneExpired(qint64 now) {
    if (retain_ms_ <= 0) return;
    points_.erase(std::remove_if(points_.begin(), points_.end(),
                                 [&](const TimedPoint& tp) {
                                     return now - tp.stamp_ms >= retain_ms_;
                                 }),
                  points_.end());
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

    // 无尾迹：整圈替换，直接画最新一圈
    if (retain_ms_ == 0) {
        if (!latest_ || latest_->points.empty()) {
            p.setPen(QColor(120, 120, 128));
            p.drawText(rect(), Qt::AlignCenter, tr("No data"));
            return;
        }
        p.setPen(Qt::NoPen);
        p.setBrush(kPointColor);
        for (const auto& pt : latest_->points) {
            if (!pt.valid) continue;
            p.drawEllipse(toScreen(pt.x_mm, pt.y_mm), point_radius_, point_radius_);
        }
        return;
    }

    // 尾迹模式：逐点按年龄淡出
    if (points_.empty()) {
        if (!latest_ || latest_->points.empty()) {
            p.setPen(QColor(120, 120, 128));
            p.drawText(rect(), Qt::AlignCenter, tr("No data"));
        }
        return;
    }

    const qint64 now = clock_.msecsSinceReference();
    pruneExpired(now);
    p.setPen(Qt::NoPen);
    for (const auto& tp : points_) {
        const double alpha = 1.0 - static_cast<double>(now - tp.stamp_ms) / retain_ms_;
        if (alpha <= 0.0) continue;
        QColor c = kPointColor;
        c.setAlphaF(alpha);
        p.setBrush(c);
        p.drawEllipse(toScreen(tp.x_mm, tp.y_mm), point_radius_, point_radius_);
    }
}

} // namespace eai
