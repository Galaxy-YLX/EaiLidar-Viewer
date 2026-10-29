#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <vector>

#include "core/scan.h"

namespace eai {

// 2D 点云俯视图：雷达位于中心，直角坐标 (x, y) 直接映射到屏幕。
// retain_ms = 0 时只显示最新一圈（整圈替换）；
// retain_ms > 0 时历史点按各自到达时间线性淡出，超过留存时间的点被移除，形成尾迹。
class ScanView : public QWidget {
    Q_OBJECT
public:
    explicit ScanView(QWidget* parent = nullptr);

    void setScan(const std::shared_ptr<const Scan>& scan);

    // 点在屏幕上的留存时间（ms）；0 = 只显示最新一圈
    void setRetainMs(int ms);
    int retainMs() const { return retain_ms_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    // 尾迹模式下带到达时间戳的点
    struct TimedPoint {
        double x_mm = 0.0;
        double y_mm = 0.0;
        qint64 stamp_ms = 0; // 到达时刻（单调时钟 msecsSinceReference）
    };

    // 一圈的点按其到达顺序在 [now - 周期, now] 内展开时间戳，追加进 points_。
    void appendScan(const std::shared_ptr<const Scan>& scan);
    void pruneExpired(qint64 now);

    QElapsedTimer clock_;
    QTimer tail_timer_;               // 驱动尾迹淡出时的重绘
    std::vector<TimedPoint> points_;  // 尾迹点（按时间顺序）
    std::shared_ptr<const Scan> latest_; // 最新一圈（retain = 0 时直接绘制）
    double range_mm_ = 10000.0;       // 视图半幅范围（mm）
    int retain_ms_ = 0;               // 点留存时间（ms）
};

} // namespace eai
