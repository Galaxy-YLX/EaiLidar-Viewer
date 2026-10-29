#pragma once

#include <QWidget>

#include <memory>
#include <vector>

#include "core/scan.h"

namespace eai {

// 2D 点云俯视图：雷达位于中心，直角坐标 (x, y) 直接映射到屏幕。
// 采用「角度槽位覆盖」刷新：每收到一帧立即把点写入对应角度的槽位
// （覆盖该角度上一次的数据），像雷达 PPI 显示一样随扫随刷。
class ScanView : public QWidget {
    Q_OBJECT
public:
    explicit ScanView(QWidget* parent = nullptr);

    void setScan(const std::shared_ptr<const Scan>& scan);

    // 当前屏幕上有效点数（供状态栏显示）
    int pointCount() const { return point_count_; }

    // 点的绘制半径（px）
    void setPointRadius(double r);
    double pointRadius() const { return point_radius_; }

    // 视图半幅范围（mm）：网格、标尺随量程缩放
    void setRangeMm(double range_mm);
    double rangeMm() const { return range_mm_; }

    // 镜像模式：0=无 1=左右(X翻转) 2=上下(Y翻转) 3=双向
    void setMirror(int mode);
    int mirror() const { return mirror_; }

    // 点云绕中心旋转（°），用于适配雷达安装方位
    void setRotationDeg(double deg);
    double rotationDeg() const { return rotation_deg_; }

    // 滤波开关（可独立组合）
    // 距离跳跃检测：相邻角度点距离差超过阈值判为噪声（默认开，100 mm）
    void setJumpFilter(bool enable, double threshold_mm);
    // 强度下限：强度低于阈值的点判为噪声（默认关）
    void setIntensityFilter(bool enable, int min_intensity);
    // 远距上限：距离超过阈值的点判为噪声（默认关）
    void setMaxDistFilter(bool enable, double max_dist_mm);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static constexpr int kSlotsPerRevolution = 1440; // 0.25°/槽，一圈 1440 槽

    // 写入槽位前对点做滤波判定；返回 true 表示该点应写入
    bool passFilter(int slot, const Point& pt) const;

    std::vector<Point> slots_; // 按角度槽位存储的点；valid=false 表示该角度无数据
    int point_count_ = 0;
    double range_mm_ = 10000.0; // 视图半幅范围（mm）
    double point_radius_ = 0.5; // 点绘制半径（px）
    int mirror_ = 1;            // 镜像模式 0/1/2/3，默认左右镜像
    double rotation_deg_ = 90.0; // 显示旋转角度（°），默认 90°

    // 滤波配置
    bool jump_enable_ = true;        // 距离跳跃检测
    double jump_threshold_mm_ = 100.0;
    bool intensity_enable_ = false;  // 强度下限
    int intensity_min_ = 20;
    bool maxdist_enable_ = false;    // 远距上限
    double maxdist_mm_ = 15000.0;
};

} // namespace eai
