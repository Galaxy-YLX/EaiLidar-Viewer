#pragma once

#include "core/scan.h"

#include <cstddef>
#include <cstdint>

namespace eai {

// 帧头（实测线路上发送：AA 55，即 0x55AA 小端）
inline constexpr std::uint8_t kPh0 = 0xAA;
inline constexpr std::uint8_t kPh1 = 0x55;

// 帧类型
enum class FrameType {
    Invalid, // 帧头 / 长度 / 校验错误
    Start,   // 起始数据包 CT[bit0] = 1
    Scan,    // 点云数据包 CT[bit0] = 0
};

// 完整帧长（字节）= 10 + 3 × LSN（每采样点 3 字节：强度 + 距离低 + 距离高）
inline std::size_t frameLength(std::uint8_t lsn) {
    return 10u + 3u * static_cast<std::size_t>(lsn);
}

// 校验：整帧所有字节异或 == 0
bool verifyChecksum(const std::uint8_t* data, std::size_t len);

// 解析一整帧。
// data 指向帧首，len 为该帧长度（应等于 frameLength(lsn)）。
// secondLevelCorrect = false 时跳过二级角度修正（对应参考配置 noSecondLevel）。
// 返回帧类型：Start 帧只填充 scan.scan_freq_hz；Scan 帧填充 fsa_deg/lsa_deg 与 points。
FrameType parseFrame(const std::uint8_t* data, std::size_t len,
                     Scan& scan, bool secondLevelCorrect = true);

} // namespace eai
