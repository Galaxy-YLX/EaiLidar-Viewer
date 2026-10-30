#include "app/mainwindow.h"

#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

#include "core/scan.h"
#include "transport/serial_transport.h"
#include "ui/scan_view.h"

namespace eai {

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
    powerOnButton_ = new QPushButton(tr("开机"), this);
    powerOffButton_ = new QPushButton(tr("关机"), this);
    powerOnButton_->setEnabled(false);
    powerOffButton_->setEnabled(false);
    powerOnButton_->setToolTip(tr("发送 A5 60 启动雷达"));
    powerOffButton_->setToolTip(tr("发送 A5 65 停止雷达"));

    // 侧边按键加大：大字号、大高度
    QFont sideFont = powerOnButton_->font();
    sideFont.setPointSize(16);
    sideFont.setBold(true);
    powerOnButton_->setFont(sideFont);
    powerOffButton_->setFont(sideFont);

    // 点大小选择：下拉选项 0.5 / 1.0 / 1.5 / 2.0 px
    sizeCombo_ = new QComboBox(this);
    for (double r : {0.5, 1.0, 1.5, 2.0}) {
        sizeCombo_->addItem(tr("%1 px").arg(r, 0, 'f', 1), r);
    }
    sizeCombo_->setCurrentIndex(0); // 默认 0.5 px
    sizeCombo_->setToolTip(tr("点的大小（半径 px）"));

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
    toolbar->addWidget(refreshButton);
    toolbar->addWidget(connectButton_);
    toolbar->addWidget(disconnectButton_);
    toolbar->addSeparator();
    toolbar->addWidget(mirrorCombo_);
    toolbar->addWidget(rotationCombo_);
    toolbar->addWidget(sizeCombo_);

    // 上边栏字号加大；控件高度由 QSS 统一控制（总高 40px）
    QFont barFont = toolbar->font();
    barFont.setPointSize(12);
    toolbar->setFont(barFont);

    // 滤波设置：距离跳跃（默认关 10cm）/ 强度下限（默认关）
    jumpCheck_ = new QCheckBox(tr("距离跳跃"), this);
    jumpCheck_->setChecked(false);
    jumpSpin_ = new QDoubleSpinBox(this);
    jumpSpin_->setRange(1, 500);
    jumpSpin_->setDecimals(0);
    jumpSpin_->setValue(10);
    jumpSpin_->setSuffix(tr(" cm"));
    intensityCheck_ = new QCheckBox(tr("强度下限"), this);
    intensitySpin_ = new QSpinBox(this);
    intensitySpin_->setRange(0, 255);
    intensitySpin_->setValue(20);

    // 侧边控制面板：开机 / 关机 / 量程 / 滤波（无标题栏、无边框，固定停靠右侧）
    auto* controlDock = new QDockWidget(tr("控制"), this);
    controlDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    controlDock->setTitleBarWidget(new QWidget(controlDock)); // 空标题栏 = 隐藏框
    auto* dockWidget = new QWidget(controlDock);
    auto* dockLayout = new QVBoxLayout(dockWidget);
    dockLayout->addWidget(powerOnButton_);
    dockLayout->addWidget(powerOffButton_);
    dockLayout->addSpacing(8);
    dockLayout->addWidget(new QLabel(tr("量程"), dockWidget));
    dockLayout->addWidget(rangeCombo_);
    dockLayout->addSpacing(12);
    dockLayout->addWidget(new QLabel(tr("滤波"), dockWidget));
    auto* jumpRow = new QHBoxLayout();
    jumpRow->addWidget(jumpCheck_);
    jumpRow->addWidget(jumpSpin_, 1);
    dockLayout->addLayout(jumpRow);
    auto* intensityRow = new QHBoxLayout();
    intensityRow->addWidget(intensityCheck_);
    intensityRow->addWidget(intensitySpin_, 1);
    dockLayout->addLayout(intensityRow);
    dockLayout->addStretch(1);
    controlDock->setWidget(dockWidget);
    addDockWidget(Qt::RightDockWidgetArea, controlDock);

    statusLabel_ = new QLabel(tr("未连接"), this);
    statusBar()->addWidget(statusLabel_);

    refreshPorts();

    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(disconnectButton_, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(powerOnButton_, &QPushButton::clicked, this, &MainWindow::onPowerOnClicked);
    connect(powerOffButton_, &QPushButton::clicked, this, &MainWindow::onPowerOffClicked);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(sizeCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        view_->setPointRadius(sizeCombo_->currentData().toDouble());
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
    // 默认：左右镜像 + 90° 旋转（信号已连接，会同步到视图）
    mirrorCombo_->setCurrentIndex(1);
    rotationCombo_->setCurrentIndex(1);

    // 滤波开关/阈值（可独立组合），任何变化同步到视图
    auto syncFilters = [this] {
        view_->setJumpFilter(jumpCheck_->isChecked(), jumpSpin_->value() * 10.0);
        view_->setIntensityFilter(intensityCheck_->isChecked(), intensitySpin_->value());
    };
    connect(jumpCheck_, &QCheckBox::toggled, this, syncFilters);
    connect(jumpSpin_, &QDoubleSpinBox::valueChanged, this, syncFilters);
    connect(intensityCheck_, &QCheckBox::toggled, this, syncFilters);
    connect(intensitySpin_, &QSpinBox::valueChanged, this, syncFilters);
    syncFilters();

    connect(transport_, &SerialTransport::scanReady, this, [this](const std::shared_ptr<Scan>& s) {
        view_->setScan(s);
        statusLabel_->setText(tr("已连接 · %1 点 · %2 Hz")
                                  .arg(QString::number(view_->pointCount()))
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

} // namespace eai
