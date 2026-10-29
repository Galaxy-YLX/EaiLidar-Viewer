#include "app/mainwindow.h"

#include <cmath>
#include <memory>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QToolBar>

#include "core/scan.h"
#include "transport/serial_transport.h"
#include "ui/scan_view.h"

namespace eai {
namespace {

constexpr double kPi = 3.14159265358979323846;

// 临时演示数据：生成一圈近圆形点云，用于无硬件时验证渲染。
std::shared_ptr<Scan> makeDemoScan() {
    auto scan = std::make_shared<Scan>();
    scan->scan_freq_hz = 6.0;
    const int n = 360;
    for (int i = 0; i < n; ++i) {
        Point pt;
        pt.angle_deg = static_cast<double>(i);
        pt.distance_mm = 3000.0 + 500.0 * std::sin(i * 0.05);
        const double rad = pt.angle_deg * kPi / 180.0;
        pt.x_mm = pt.distance_mm * std::cos(rad);
        pt.y_mm = pt.distance_mm * std::sin(rad);
        pt.valid = true;
        scan->points.push_back(pt);
    }
    return scan;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    view_ = new ScanView(this);
    transport_ = new SerialTransport(this);
    setCentralWidget(view_);
    setWindowTitle(tr("EaiLidar Viewer"));
    resize(800, 600);

    auto* toolbar = addToolBar(tr("串口"));
    toolbar->setMovable(false);

    portCombo_ = new QComboBox(this);
    portCombo_->setEditable(true);

    baudCombo_ = new QComboBox(this);
    const int bauds[] = {115200, 150000, 230400, 460800, 512000, 921600};
    for (int b : bauds) {
        baudCombo_->addItem(QString::number(b), b);
    }
    baudCombo_->setCurrentText(QStringLiteral("115200"));

    connectButton_ = new QPushButton(tr("连接"), this);
    disconnectButton_ = new QPushButton(tr("断开"), this);
    disconnectButton_->setEnabled(false);
    auto* refreshButton = new QPushButton(tr("刷新"), this);
    auto* demoButton = new QPushButton(tr("演示"), this);
    powerOnButton_ = new QPushButton(tr("开机"), this);
    powerOffButton_ = new QPushButton(tr("关机"), this);
    powerOnButton_->setEnabled(false);
    powerOffButton_->setEnabled(false);
    powerOnButton_->setToolTip(tr("发送 A5 60 启动雷达"));
    powerOffButton_->setToolTip(tr("发送 A5 65 停止雷达"));

    // 点大小调节：半径 0.1 ~ 5.0 px，直接输入
    sizeSpin_ = new QDoubleSpinBox(this);
    sizeSpin_->setRange(0.1, 5.0);
    sizeSpin_->setSingleStep(0.1);
    sizeSpin_->setDecimals(1);
    sizeSpin_->setValue(1.0);
    sizeSpin_->setSuffix(tr(" px"));
    sizeSpin_->setFixedWidth(76);
    sizeSpin_->setToolTip(tr("点的大小（半径 px）"));

    // 量程选择：小范围探测可缩小到 1 m，标尺/网格步进自动变细；雷达最大量程 20 m
    rangeCombo_ = new QComboBox(this);
    const int ranges_m[] = {1, 2, 5, 10, 20};
    for (int m : ranges_m) {
        rangeCombo_->addItem(tr("%1 m").arg(m), m * 1000.0);
    }
    rangeCombo_->setCurrentText(QStringLiteral("10 m"));
    rangeCombo_->setToolTip(tr("视图量程（半径），最大 20 m"));

    // 镜像与旋转：适配雷达安装方位（正装/倒装/侧装）
    mirrorCombo_ = new QComboBox(this);
    mirrorCombo_->addItem(tr("无镜像"), 0);
    mirrorCombo_->addItem(tr("左右镜像"), 1);
    mirrorCombo_->addItem(tr("上下镜像"), 2);
    mirrorCombo_->addItem(tr("双向镜像"), 3);
    mirrorCombo_->setToolTip(tr("点云镜像显示"));

    rotationCombo_ = new QComboBox(this);
    for (int deg : {0, 90, 180, 270}) {
        rotationCombo_->addItem(tr("%1°").arg(deg), deg);
    }
    rotationCombo_->setToolTip(tr("点云旋转显示"));

    toolbar->addWidget(new QLabel(tr(" 端口 "), this));
    toolbar->addWidget(portCombo_);
    toolbar->addWidget(new QLabel(tr(" 波特率 "), this));
    toolbar->addWidget(baudCombo_);
    toolbar->addWidget(connectButton_);
    toolbar->addWidget(disconnectButton_);
    toolbar->addWidget(refreshButton);
    toolbar->addWidget(demoButton);
    toolbar->addWidget(powerOnButton_);
    toolbar->addWidget(powerOffButton_);
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel(tr(" 量程 "), this));
    toolbar->addWidget(rangeCombo_);
    toolbar->addWidget(mirrorCombo_);
    toolbar->addWidget(rotationCombo_);
    toolbar->addWidget(sizeSpin_);

    statusLabel_ = new QLabel(tr("未连接"), this);
    statusBar()->addWidget(statusLabel_);

    refreshPorts();

    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(disconnectButton_, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(powerOnButton_, &QPushButton::clicked, this, &MainWindow::onPowerOnClicked);
    connect(powerOffButton_, &QPushButton::clicked, this, &MainWindow::onPowerOffClicked);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(demoButton, &QPushButton::clicked, this, &MainWindow::loadDemoScan);
    connect(sizeSpin_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        view_->setPointRadius(v);
    });
    connect(rangeCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        view_->setRangeMm(rangeCombo_->currentData().toDouble());
    });
    connect(mirrorCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        view_->setMirror(mirrorCombo_->currentData().toInt());
    });
    connect(rotationCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        view_->setRotationDeg(rotationCombo_->currentData().toDouble());
    });

    connect(transport_, &SerialTransport::scanReady, this, [this](const std::shared_ptr<Scan>& s) {
        view_->setScan(s);
        statusLabel_->setText(tr("已连接 · %1 点 · %2 Hz")
                                  .arg(QString::number(s->points.size()))
                                  .arg(QString::number(s->scan_freq_hz, 'f', 1)));
    });
    connect(transport_, &SerialTransport::frameError, this, [this](const QString& msg) {
        statusLabel_->setText(msg);
    });
}

void MainWindow::refreshPorts() {
    const QString cur = portCombo_->currentText();
    portCombo_->clear();
    portCombo_->addItems(SerialTransport::availablePorts());
    if (!cur.isEmpty()) {
        const int idx = portCombo_->findText(cur);
        if (idx >= 0) portCombo_->setCurrentIndex(idx);
    }
    if (portCombo_->count() == 0) {
        statusLabel_->setText(tr("未检测到串口，点击「刷新」重试"));
    }
}

void MainWindow::onConnectClicked() {
    if (transport_->isOpen()) {
        return;
    }

    const QString port = portCombo_->currentText().trimmed();
    if (port.isEmpty()) {
        statusLabel_->setText(tr("请先选择串口"));
        return;
    }
    const int baud = baudCombo_->currentData().toInt();
    if (transport_->open(port, baud)) {
        connectButton_->setEnabled(false);
        disconnectButton_->setEnabled(true);
        powerOnButton_->setEnabled(true);
        powerOffButton_->setEnabled(false);
        statusLabel_->setText(tr("已连接 %1 @ %2").arg(port).arg(baud));
    } else {
        statusLabel_->setText(tr("打开失败：%1").arg(transport_->errorString()));
    }
}

void MainWindow::onDisconnectClicked() {
    // 雷达仍开机时先发关机命令再断串口（仿参考软件：停止测试先停雷达）
    if (powerOffButton_->isEnabled()) {
        transport_->powerOff();
    }
    transport_->close();
    connectButton_->setEnabled(true);
    disconnectButton_->setEnabled(false);
    powerOnButton_->setEnabled(false);
    powerOffButton_->setEnabled(false);
    statusLabel_->setText(tr("已断开"));
    view_->setScan(nullptr);
}

void MainWindow::onPowerOnClicked() {
    if (!transport_->isOpen()) {
        statusLabel_->setText(tr("请先连接串口"));
        return;
    }
    if (transport_->powerOn()) {
        powerOnButton_->setEnabled(false);
        powerOffButton_->setEnabled(true);
        statusLabel_->setText(tr("雷达已开机"));
    } else {
        statusLabel_->setText(tr("开机命令发送失败"));
    }
}

void MainWindow::onPowerOffClicked() {
    if (!transport_->isOpen()) {
        statusLabel_->setText(tr("请先连接串口"));
        return;
    }
    if (transport_->powerOff()) {
        powerOnButton_->setEnabled(true);
        powerOffButton_->setEnabled(false);
        statusLabel_->setText(tr("雷达已关机"));
    } else {
        statusLabel_->setText(tr("关机命令发送失败"));
    }
}

void MainWindow::loadDemoScan() {
    if (transport_->isOpen()) {
        statusLabel_->setText(tr("请先断开串口再载入演示"));
        return;
    }
    view_->setScan(makeDemoScan());
    statusLabel_->setText(tr("演示数据"));
}

} // namespace eai
