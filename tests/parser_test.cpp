#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "protocol/frame_assembler.h"
#include "protocol/parser.h"

namespace {

int g_failures = 0;

#define CHECK(cond, ...)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            std::printf("FAIL: %s:%d  ", __FILE__, __LINE__);         \
            std::printf(__VA_ARGS__);                                 \
            std::printf("\n");                                        \
            ++g_failures;                                             \
        }                                                             \
    } while (0)

bool near(double a, double b, double eps = 1e-3) {
    return std::fabs(a - b) < eps;
}

// 从十六进制字符串构造字节（空格分隔）
std::vector<std::uint8_t> hexBytes(const std::string& hex) {
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i + 1 < hex.size(); ++i) {
        if (hex[i] == ' ') continue;
        const int hi = nib(hex[i]);
        const int lo = nib(hex[i + 1]);
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
        ++i;
    }
    return out;
}

// 构造一帧：PH(2) + CT + LSN + FSA + LSA + CS(2) + 3×LSN 采样（强度 + 距离低 + 距离高）。
// CS 自动计算，使整帧异或 == 0。
std::vector<std::uint8_t> makeFrame(std::uint8_t ct, std::uint8_t lsn,
                                    std::uint16_t fsa, std::uint16_t lsa,
                                    const std::vector<std::array<std::uint8_t, 3>>& samples) {
    std::vector<std::uint8_t> buf;
    buf.push_back(0xAA);
    buf.push_back(0x55);
    buf.push_back(ct);
    buf.push_back(lsn);
    buf.push_back(static_cast<std::uint8_t>(fsa & 0xFF));
    buf.push_back(static_cast<std::uint8_t>(fsa >> 8));
    buf.push_back(static_cast<std::uint8_t>(lsa & 0xFF));
    buf.push_back(static_cast<std::uint8_t>(lsa >> 8));
    buf.push_back(0); // CS 低字节占位
    buf.push_back(0); // CS 高字节占位
    for (const auto& s : samples) {
        buf.push_back(s[0]);
        buf.push_back(s[1]);
        buf.push_back(s[2]);
    }
    std::uint8_t x = 0;
    for (std::uint8_t b : buf) x ^= b;
    buf[8] = x; // 使整帧异或 == 0
    buf[9] = 0;
    return buf;
}

} // namespace

int main() {
    using namespace eai;

    // 0. 帧长
    CHECK(frameLength(8) == 34, "frameLength(8) should be 34, got %zu", frameLength(8));
    CHECK(frameLength(1) == 13, "frameLength(1) should be 13, got %zu", frameLength(1));

    // 1. 帧头错误
    {
        Scan scan;
        std::uint8_t bad[] = {0x00, 0x55, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        CHECK(parseFrame(bad, sizeof(bad), scan) == FrameType::Invalid,
              "bad header should be Invalid");
    }

    // 2. 真实帧 #0（来自实测数据流）
    {
        auto fr = hexBytes("AA 55 00 08 F9 57 6D 5B E3 51 BF D4 08 41 00 09 "
                           "15 E4 70 15 9C 71 1C 08 30 32 0C 30 32 C0 2F 2F 78 2F");
        CHECK(verifyChecksum(fr.data(), fr.size()), "real frame checksum should pass");

        Scan scan;
        CHECK(parseFrame(fr.data(), fr.size(), scan) == FrameType::Scan,
              "real frame should be Scan");
        CHECK(scan.points.size() == 8, "8 points, got %zu", scan.points.size());
        // FSA = 0x57F9 >> 1 / 64 = 175.9375
        CHECK(near(scan.fsa_deg, 175.9375, 1e-3), "FSA ~= 175.9375, got %.5f", scan.fsa_deg);
        // LSA = 0x5B6D >> 1 / 64 = 182.84375
        CHECK(near(scan.lsa_deg, 182.84375, 1e-3), "LSA ~= 182.84375, got %.5f", scan.lsa_deg);

        CHECK(scan.points[0].intensity == 191, "S0 intensity 191, got %d", scan.points[0].intensity);
        CHECK(near(scan.points[0].distance_mm, 565.0), "S0 dist 565, got %.2f",
              scan.points[0].distance_mm);
        CHECK(scan.points[2].intensity == 21, "S2 intensity 21, got %d", scan.points[2].intensity);
        CHECK(near(scan.points[2].distance_mm, 7225.0), "S2 dist 7225, got %.2f",
              scan.points[2].distance_mm);
        CHECK(near(scan.points[7].distance_mm, 3038.0), "S7 dist 3038, got %.2f",
              scan.points[7].distance_mm);
    }

    // 3. 起始数据包：CT=0x8D → (0x8D>>1)/10 = 7.0 Hz
    {
        auto buf = makeFrame(0x8D, 1, 0, 0, {{{0, 0, 0}}});
        Scan scan;
        CHECK(parseFrame(buf.data(), buf.size(), scan) == FrameType::Start,
              "CT=0x8D should be Start");
        CHECK(near(scan.scan_freq_hz, 7.0), "freq should be 7.0, got %.3f", scan.scan_freq_hz);
    }

    // 4. 校验失败
    {
        auto fr = hexBytes("AA 55 00 08 F9 57 6D 5B E3 51 BF D4 08 41 00 09 "
                           "15 E4 70 15 9C 71 1C 08 30 32 0C 30 32 C0 2F 2F 78 2F");
        fr[10] ^= 0xFF; // 破坏一个字节
        Scan scan;
        CHECK(parseFrame(fr.data(), fr.size(), scan) == FrameType::Invalid,
              "corrupted checksum should be Invalid");
    }

    // 5. 组帧器：垃圾字节 + 分片喂入，应恰好输出一帧
    {
        auto frame = hexBytes("AA 55 00 08 F9 57 6D 5B E3 51 BF D4 08 41 00 09 "
                              "15 E4 70 15 9C 71 1C 08 30 32 0C 30 32 C0 2F 2F 78 2F");
        std::vector<std::uint8_t> full;
        full.push_back(0xFF);
        full.push_back(0xEE); // 垃圾字节
        full.insert(full.end(), frame.begin(), frame.end());

        int count = 0;
        std::size_t lastLen = 0;
        FrameAssembler assembler([&](const std::uint8_t* d, std::size_t len) {
            ++count;
            lastLen = len;
            CHECK(d[0] == 0xAA && d[1] == 0x55, "assembler emitted wrong header");
        });
        assembler.push(full.data(), 3);                   // FF EE AA
        assembler.push(full.data() + 3, full.size() - 3); // 55 + 帧剩余
        CHECK(count == 1, "should emit exactly 1 frame, got %d", count);
        CHECK(lastLen == frame.size(), "frame len should be %zu, got %zu",
              frame.size(), lastLen);
    }

    if (g_failures == 0) {
        std::printf("All parser tests passed.\n");
        return 0;
    }
    std::printf("%d parser test(s) failed.\n", g_failures);
    return 1;
}
