#pragma once

namespace eai {

// 单个采样点（已换算到直角坐标）
struct Point {
    double angle_deg = 0.0;   // 二级修正后的角度（°）
    double distance_mm = 0.0; // 距离（mm）
    double x_mm = 0.0;        // 直角坐标 X
    double y_mm = 0.0;        // 直角坐标 Y
    bool valid = false;       // 距离 > 0 且未被干扰标志标记
};

} // namespace eai
