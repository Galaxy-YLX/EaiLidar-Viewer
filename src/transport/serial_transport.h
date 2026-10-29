#pragma once

#include <QObject>
#include <QSerialPort>
#include <QStringList>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "core/scan.h"
#include "protocol/frame_assembler.h"

namespace eai {

// 串口传输 + 帧解析：读字节流 → 组帧 → 解析成 Scan → 发出 scanReady 信号。
// 基于 readyRead 异步非阻塞，运行在主线程；数据量明显增大时再迁到工作线程。
class SerialTransport : public QObject {
    Q_OBJECT
public:
    explicit SerialTransport(QObject* parent = nullptr);
    ~SerialTransport() override;

    // 列出可用串口（如 /dev/ttyUSB0、COM3）
    static QStringList availablePorts();

    // 打开串口，返回是否成功；失败时用 errorString() 取原因。
    bool open(const QString& portName, int baudRate = 115200);
    void close();
    bool isOpen() const;
    QString errorString() const;

    // 雷达开机/关机命令帧（需串口已打开），返回是否发送成功。
    bool powerOn();
    bool powerOff();

signals:
    void scanReady(const std::shared_ptr<eai::Scan>& scan);
    void frameError(const QString& message);
    void opened(bool ok);

private slots:
    void onReadyRead();

private:
    void onFrame(const std::uint8_t* data, std::size_t len);
    void flushPending();

    QSerialPort serial_;
    FrameAssembler assembler_;
    double last_freq_hz_ = 0.0;
    Scan pending_; // 跨帧累积一整圈的点，遇到起始帧或超上限时整体发出
};

} // namespace eai
