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
    slots_.resize(kSlotsPerRevolution);
    stamps_.resize(kSlotsPerRevolution);

    // 余辉：周期性清除过期点并重绘，使拖影随时间消失
    decayTimer_.setInterval(50);
    connect(&decayTimer_, &QTimer::timeout, this, [this] {
        decayExpired();
        update();
    });
}

void ScanView::setScan(const std::shared_ptr<const Scan>& scan) {
    if (scan == nullptr) {
        slots_.assign(kSlotsPerRevolution, Point{});
        point_count_ = 0;
        update();
        return;
    }

    scan_freq_hz_ = scan->scan_freq_hz;

    // 先清除已过期的点，避免陈旧数据残留（拖影）
    decayExpired();

    // 逐点按其角度写入槽位，覆盖该角度上一次的数据
    const auto now = std::chrono::steady_clock::now();
    for (const auto& pt : scan->points) {
        double ang = std::fmod(pt.angle_deg, 360.0);
        if (ang < 0.0) ang += 360.0;
        const int slot = static_cast<int>(std::lround(ang / 360.0 * kSlotsPerRevolution)) %
                         kSlotsPerRevolution;

        // 滤波判定：不通过的判为噪声，写入无效点（覆盖擦除该角度旧数据）
        const bool pass = passFilter(slot, pt);
        Point& dst = slots_[slot];
        const bool wasValid = dst.valid;
        dst = pt;
        dst.angle_deg = ang; // 归一化角度
        if (!pass) {
            dst.valid = false;
        }
        stamps_[slot] = now; // 记录刷新时刻（无论是否有效，均代表该角度已更新）
        if (wasValid != dst.valid) {
            point_count_ += dst.valid ? 1 : -1;
        }
    }
    update();
}

bool ScanView::passFilter(int slot, const Point& pt) const {
    if (!pt.valid) return false;

    // 强度下限
    if (intensity_enable_ && pt.intensity < intensity_min_) return false;

    // 距离跳跃检测：与相邻槽位的有效点比较，距离差超阈值判为噪声
    if (jump_enable_) {
        for (int off : {-1, 1}) {
            const int ns = (slot + off + kSlotsPerRevolution) % kSlotsPerRevolution;
            const Point& n = slots_[ns];
            if (n.valid && std::abs(n.distance_mm - pt.distance_mm) > jump_threshold_mm_) {
                return false;
            }
        }
    }
    return true;
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

void ScanView::setJumpFilter(bool enable, double threshold_mm) {
    jump_enable_ = enable;
    jump_threshold_mm_ = std::max(1.0, threshold_mm);
    update();
}

void ScanView::setIntensityFilter(bool enable, int min_intensity) {
    intensity_enable_ = enable;
    intensity_min_ = std::clamp(min_intensity, 0, 255);
    update();
}

void ScanView::setPersistence(bool enable, int revolutions) {
    persistence_enable_ = enable;
    persistence_revs_ = std::max(1, revolutions);
    if (enable) {
        decayTimer_.start();
    } else {
        decayTimer_.stop();
    }
    update();
}

void ScanView::setGateFilter(bool enable, double gate_mm) {
    gate_enable_ = enable;
    gate_mm_ = std::max(1.0, gate_mm);
    update();
}

void ScanView::decayExpired() {
    if (!persistence_enable_) return;

    // 一圈时长 = 1000 / 扫描频率；频率未知时按 10 Hz 估计
    const double period_ms = scan_freq_hz_ > 0.0 ? 1000.0 / scan_freq_hz_ : 100.0;
    const double threshold_ms = persistence_revs_ * period_ms;

    const auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < kSlotsPerRevolution; ++i) {
        Point& p = slots_[i];
        if (!p.valid) continue;
        const double age_ms =
            std::chrono::duration<double, std::milli>(now - stamps_[i]).count();
        if (age_ms <= threshold_ms) continue;
        // 过期候选：开启门限验证且距离门内存在新点，则视为关联上、不清除
        if (gate_enable_ && gateMatched(i, threshold_ms, now)) continue;
        p.valid = false;
        --point_count_;
    }
}

bool ScanView::gateMatched(int slot, double threshold_ms,
                           std::chrono::steady_clock::time_point now) const {
    const Point& p = slots_[slot];
    const double gate2 = gate_mm_ * gate_mm_;
    for (int j = 0; j < kSlotsPerRevolution; ++j) {
        if (j == slot) continue;
        const Point& q = slots_[j];
        if (!q.valid) continue;
        const double age_ms =
            std::chrono::duration<double, std::milli>(now - stamps_[j]).count();
        if (age_ms > threshold_ms) continue; // 只与未过期的新点关联
        const double dx = p.x_mm - q.x_mm;
        const double dy = p.y_mm - q.y_mm;
        if (dx * dx + dy * dy <= gate2) return true;
    }
    return false;
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

    // 画槽位中的有效点
    if (point_count_ == 0) {
        p.setPen(QColor(60, 92, 68));
        p.drawText(rect(), Qt::AlignCenter, tr("No data"));
        return;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(kPointColor);
    for (const auto& pt : slots_) {
        if (!pt.valid) continue;
        p.drawEllipse(toScreen(pt.x_mm, pt.y_mm), point_radius_, point_radius_);
    }
}

} // namespace eai
