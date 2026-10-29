#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace eai {

// 把串口字节流切成完整帧（找帧头 + 按 LSN 定长）。
// 内部自动重同步：找不到帧头时逐字节跳过，收到完整帧后回调。
class FrameAssembler {
public:
    using FrameCallback = std::function<void(const std::uint8_t* data, std::size_t len)>;

    explicit FrameAssembler(FrameCallback onFrame) : onFrame_(std::move(onFrame)) {}

    // 喂入一段字节，可能触发 0 次或多次 onFrame 回调。
    void push(const std::uint8_t* data, std::size_t len);

    void reset() { buf_.clear(); }

private:
    FrameCallback onFrame_;
    std::vector<std::uint8_t> buf_;
};

} // namespace eai
