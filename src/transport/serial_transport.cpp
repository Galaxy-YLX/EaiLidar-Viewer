#include "transport/serial_transport.h"

#include <QSerialPortInfo>

#include "protocol/parser.h"

namespace eai {
namespace {

// 雷达控制命令帧（实测）：开机 A5 60，关机 A5 65。
// 注：与数据帧头 AA 55 不同，命令帧头是 A5。
const QByteArray kPowerOnCmd("\xA5\x60", 2);
const QByteArray kPowerOffCmd("\xA5\x65", 2);

} // namespace

SerialTransport::SerialTransport(QObject* parent)
    : QObject(parent),
      assembler_([this](const std::uint8_t* data, std::size_t len) {
          onFrame(data, len);
      }) {
    serial_.setDataBits(QSerialPort::Data8);
    serial_.setParity(QSerialPort::NoParity);
    serial_.setStopBits(QSerialPort::OneStop);
    serial_.setFlowControl(QSerialPort::NoFlowControl);

    connect(&serial_, &QSerialPort::readyRead, this, &SerialTransport::onReadyRead);
    connect(&serial_, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError err) {
        if (err != QSerialPort::NoError) {
            emit frameError(serial_.errorString());
        }
    });
}

SerialTransport::~SerialTransport() {
    close();
}

QStringList SerialTransport::availablePorts() {
    QStringList names;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto& p : ports) {
        names << p.portName();
    }
    return names;
}

bool SerialTransport::open(const QString& portName, int baudRate) {
    serial_.setPortName(portName);
    serial_.setBaudRate(baudRate);

    if (!serial_.open(QIODevice::ReadWrite)) {
        return false;
    }
    assembler_.reset();
    last_freq_hz_ = 0.0;
    emit opened(true);
    return true;
}

void SerialTransport::close() {
    if (serial_.isOpen()) {
        serial_.close();
    }
}

bool SerialTransport::isOpen() const {
    return serial_.isOpen();
}

QString SerialTransport::errorString() const {
    return serial_.errorString();
}

bool SerialTransport::powerOn() {
    if (!serial_.isOpen()) return false;
    return serial_.write(kPowerOnCmd) == kPowerOnCmd.size();
}

bool SerialTransport::powerOff() {
    if (!serial_.isOpen()) return false;
    return serial_.write(kPowerOffCmd) == kPowerOffCmd.size();
}

void SerialTransport::onReadyRead() {
    const QByteArray data = serial_.readAll();
    if (data.isEmpty()) return;
    assembler_.push(reinterpret_cast<const std::uint8_t*>(data.constData()),
                    static_cast<std::size_t>(data.size()));
}

void SerialTransport::onFrame(const std::uint8_t* data, std::size_t len) {
    Scan scan;
    const FrameType type = parseFrame(data, len, scan, /*secondLevelCorrect=*/true);
    switch (type) {
    case FrameType::Start:
        last_freq_hz_ = scan.scan_freq_hz;
        break;
    case FrameType::Scan: {
        // 逐帧立即发出，显示层按角度覆盖刷新
        scan.scan_freq_hz = last_freq_hz_;
        emit scanReady(std::make_shared<Scan>(std::move(scan)));
        break;
    }
    case FrameType::Invalid:
        emit frameError(tr("无效帧（帧头/长度错误）"));
        break;
    }
}

} // namespace eai
