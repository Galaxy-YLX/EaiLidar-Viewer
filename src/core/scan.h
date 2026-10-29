#pragma once

#include "core/point.h"

#include <vector>

namespace eai {

// 一圈（一帧）扫描数据
struct Scan {
    double fsa_deg = 0.0;       // 起始角（一级解析，未修正）
    double lsa_deg = 0.0;       // 结束角（一级解析，未修正）
    double scan_freq_hz = 0.0;  // 扫描频率（来自起始数据包）
    std::vector<Point> points;  // 每个点的 angle_deg 已含二级修正
};

} // namespace eai
