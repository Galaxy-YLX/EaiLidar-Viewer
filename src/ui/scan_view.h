#pragma once

#include <QWidget>

#include <memory>

#include "core/scan.h"

namespace eai {

// 2D 点云俯视图：雷达位于中心，直角坐标 (x, y) 直接映射到屏幕。
class ScanView : public QWidget {
    Q_OBJECT
public:
    explicit ScanView(QWidget* parent = nullptr);

    void setScan(const std::shared_ptr<const Scan>& scan);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::shared_ptr<const Scan> scan_;
    double range_mm_ = 10000.0; // 视图半幅范围（mm）
};

} // namespace eai
