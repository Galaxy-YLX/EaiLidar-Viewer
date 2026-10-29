#include "protocol/parser.h"

#include <cmath>

namespace eai {
namespace {

constexpr double kPi = 3.14159265358979323846;

std::uint16_t readU16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

// 二级角度修正值（°）：
// IF d == 0 : 0  ELSE : tan⁻¹[ 21.8 × (155.3 - d) / (155.3 × d) ]
double angleCorrection(double distance_mm) {
    if (distance_mm <= 0.0) return 0.0;
    const double v = 21.8 * (155.3 - distance_mm) / (155.3 * distance_mm);
    return std::atan(v) * 180.0 / kPi;
}

} // namespace

bool verifyChecksum(const std::uint8_t* data, std::size_t len) {
    if (data == nullptr || len == 0) return false;
    std::uint8_t x = 0;
    for (std::size_t i = 0; i < len; ++i) x ^= data[i];
    return x == 0;
}

FrameType parseFrame(const std::uint8_t* data, std::size_t len,
                     Scan& scan, bool secondLevelCorrect) {
    scan = Scan{};
    if (data == nullptr || len < 10) return FrameType::Invalid;
    if (data[0] != kPh0 || data[1] != kPh1) return FrameType::Invalid;

    const std::uint8_t ct = data[2];
    const std::uint8_t lsn = data[3];
    if (len < frameLength(lsn)) return FrameType::Invalid;

    // 整帧异或校验
    if (!verifyChecksum(data, frameLength(lsn))) return FrameType::Invalid;

    const std::uint16_t fsa = readU16(data + 4);
    const std::uint16_t lsa = readU16(data + 6);

    // 起始数据包：LSN = 1，CT[bit7:1] 承载扫描频率
    if (ct & 0x01u) {
        scan.scan_freq_hz = (ct >> 1) / 10.0;
        return FrameType::Start;
    }

    // 点云数据包
    const double a0 = (fsa >> 1) / 64.0; // 一级起始角（°）
    const double a1 = (lsa >> 1) / 64.0; // 一级结束角（°）
    double diff = a1 - a0;
    if (diff < 0.0) diff += 360.0; // 跨零点按顺时针角度差

    scan.fsa_deg = a0;
    scan.lsa_deg = a1;
    scan.points.reserve(lsn);

    for (std::size_t i = 0; i < lsn; ++i) {
        const std::uint8_t* s = data + 10 + 3 * i;
        Point pt;
        pt.intensity = s[0];
        const std::uint16_t si = static_cast<std::uint16_t>(s[1]) |
                                 (static_cast<std::uint16_t>(s[2]) << 8);
        pt.distance_mm = si / 4.0; // 距离（mm）；距离字段低 bit 为干扰标志
        pt.valid = pt.distance_mm > 0.0;

        // 一级角度：首点取 a0，其余在 [a0, a1] 间线性插值
        double ang = (i == 0) ? a0 : a0 + diff / static_cast<double>(lsn - 1) * i;
        if (secondLevelCorrect && pt.valid) {
            ang += angleCorrection(pt.distance_mm);
        }
        pt.angle_deg = ang;

        const double rad = ang * kPi / 180.0;
        pt.x_mm = pt.distance_mm * std::cos(rad);
        pt.y_mm = pt.distance_mm * std::sin(rad);
        scan.points.push_back(pt);
    }

    return FrameType::Scan;
}

} // namespace eai
