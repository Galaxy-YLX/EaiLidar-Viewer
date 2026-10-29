#include <cmath>
#include <cstdint>
#include <cstdio>
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

// 构造一帧：PH(2) + CT + LSN + FSA + LSA + CS + S1..S_LSN
std::vector<std::uint8_t> makeScanFrame(std::uint8_t ct, std::uint8_t lsn,
                                        std::uint16_t fsa, std::uint16_t lsa,
                                        const std::vector<std::uint16_t>& samples) {
    std::vector<std::uint8_t> buf;
    buf.push_back(0xA5);
    buf.push_back(0x55);
    buf.push_back(ct);
    buf.push_back(lsn);
    buf.push_back(static_cast<std::uint8_t>(fsa & 0xFF));
    buf.push_back(static_cast<std::uint8_t>(fsa >> 8));
    buf.push_back(static_cast<std::uint8_t>(lsa & 0xFF));
    buf.push_back(static_cast<std::uint8_t>(lsa >> 8));
    buf.push_back(0); // CS 低字节
    buf.push_back(0); // CS 高字节
    for (std::uint16_t s : samples) {
        buf.push_back(static_cast<std::uint8_t>(s & 0xFF));
        buf.push_back(static_cast<std::uint8_t>(s >> 8));
    }
    return buf;
}

} // namespace

int main() {
    using namespace eai;

    // 1. 帧头错误
    {
        Scan scan;
        std::uint8_t bad[] = {0x00, 0x55, 0, 0, 0, 0, 0, 0, 0, 0};
        CHECK(parseFrame(bad, sizeof(bad), scan) == FrameType::Invalid,
              "bad header should be Invalid");
    }

    // 2. 起始数据包：CT=0x15 → bit0=1, bit7:1=0b1010=10 → 1.0 Hz
    {
        std::vector<std::uint8_t> buf = makeScanFrame(0x15, 1, 0, 0, {0});
        Scan scan;
        CHECK(parseFrame(buf.data(), buf.size(), scan) == FrameType::Start,
              "CT=0x15 should be Start");
        CHECK(near(scan.scan_freq_hz, 1.0), "freq should be 1.0, got %.4f", scan.scan_freq_hz);
    }

    // 3. 点云帧头解析（协议.md 算例）：LSN=40, FSA=0x6FE5, LSA=0x79ED
    {
        std::vector<std::uint16_t> samples(40, 0x1000);
        std::vector<std::uint8_t> buf = makeScanFrame(0x00, 40, 0x6FE5, 0x79ED, samples);
        Scan scan;
        CHECK(parseFrame(buf.data(), buf.size(), scan) == FrameType::Scan,
              "should be Scan");
        CHECK(scan.points.size() == 40, "points should be 40, got %zu", scan.points.size());
        // FSA = (0x6FE5 >> 1) / 64 = 14322 / 64 = 223.78125
        CHECK(near(scan.fsa_deg, 223.78125), "FSA should be 223.78125, got %.5f", scan.fsa_deg);
        // LSA = (0x79ED >> 1) / 64 = 15606 / 64 = 243.84375（协议.md 原文 243.47 系笔误）
        CHECK(near(scan.lsa_deg, 243.84375), "LSA should be 243.84375, got %.5f", scan.lsa_deg);
    }

    // 4. 距离解算：Si = 0x6FE5 → 7161.25 mm
    {
        std::vector<std::uint8_t> buf = makeScanFrame(0x00, 1, 0, 0, {0x6FE5});
        Scan scan;
        parseFrame(buf.data(), buf.size(), scan);
        CHECK(near(scan.points[0].distance_mm, 7161.25),
              "distance should be 7161.25, got %.4f", scan.points[0].distance_mm);
    }

    // 5. 二级角度修正：d=1000 → -6.7622°, d=8000 → -7.8374°
    {
        std::vector<std::uint8_t> buf = makeScanFrame(
            0x00, 2, 0, 0, {static_cast<std::uint16_t>(1000 * 4),
                            static_cast<std::uint16_t>(8000 * 4)});
        Scan scan;
        parseFrame(buf.data(), buf.size(), scan);
        CHECK(near(scan.points[0].distance_mm, 1000.0), "d0 should be 1000");
        CHECK(near(scan.points[1].distance_mm, 8000.0), "d1 should be 8000");
        // FSA=LSA=0，一级角度为 0，故 point.angle_deg 即修正值本身
        CHECK(near(scan.points[0].angle_deg, -6.7622, 1e-3),
              "corr(1000) should be -6.7622, got %.4f", scan.points[0].angle_deg);
        CHECK(near(scan.points[1].angle_deg, -7.8374, 1e-3),
              "corr(8000) should be -7.8374, got %.4f", scan.points[1].angle_deg);
    }

    // 6. 组帧器：垃圾字节 + 分片喂入，应恰好输出一帧并重同步
    {
        std::vector<std::uint8_t> frame = makeScanFrame(0x00, 1, 0, 0, {0x6FE5});
        std::vector<std::uint8_t> full;
        full.push_back(0xFF);
        full.push_back(0xEE); // 垃圾字节
        full.insert(full.end(), frame.begin(), frame.end());

        int count = 0;
        std::size_t lastLen = 0;
        FrameAssembler assembler([&](const std::uint8_t* d, std::size_t len) {
            ++count;
            lastLen = len;
            CHECK(d[0] == 0xA5 && d[1] == 0x55, "assembler emitted wrong header");
        });
        assembler.push(full.data(), 3);                      // FF EE A5
        assembler.push(full.data() + 3, full.size() - 3);    // 55 + 帧剩余
        CHECK(count == 1, "should emit exactly 1 frame, got %d", count);
        CHECK(lastLen == frame.size(), "frame len should be %zu, got %zu", frame.size(), lastLen);
    }

    if (g_failures == 0) {
        std::printf("All parser tests passed.\n");
        return 0;
    }
    std::printf("%d parser test(s) failed.\n", g_failures);
    return 1;
}
