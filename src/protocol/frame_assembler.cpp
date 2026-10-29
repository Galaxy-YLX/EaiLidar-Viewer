#include "protocol/frame_assembler.h"
#include "protocol/parser.h"

namespace eai {

void FrameAssembler::push(const std::uint8_t* data, std::size_t len) {
    if (data == nullptr || len == 0) return;
    buf_.insert(buf_.end(), data, data + len);

    std::size_t i = 0;
    while (i + 4 <= buf_.size()) {
        if (buf_[i] != kPh0 || buf_[i + 1] != kPh1) {
            ++i;
            continue;
        }
        const std::uint8_t lsn = buf_[i + 3];
        const std::size_t frameLen = frameLength(lsn);
        if (i + frameLen > buf_.size()) break; // 帧尚未收完
        onFrame_(buf_.data() + i, frameLen);
        i += frameLen;
    }

    // 丢弃已消费字节，保留未消费尾部（可能含半个帧头）
    if (i > 0) buf_.erase(buf_.begin(), buf_.begin() + i);
}

} // namespace eai
