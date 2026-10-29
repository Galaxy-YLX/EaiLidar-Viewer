#include "app/mainwindow.h"

#include <cmath>
#include <memory>

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
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

    startButton_ = new QPushButton(tr("启动"), this);
    stopButton_ = new QPushButton(tr("关闭"), this);
    stopButton_->setEnabled(false);
    auto* refreshButton = new QPushButton(tr("刷新"), this);
    auto* demoButton = new QPushButton(tr("演示"), this);

    // 点留存时间调节：0 = 只显示最新一圈，>0 = 历史点渐隐尾迹
    retainSlider_ = new QSlider(Qt::Horizontal, this);
    retainSlider_->setRange(0, 3000); // 0 ~ 3000 ms
    retainSlider_->setSingleStep(100);
    retainSlider_->setPageStep(500);
    retainSlider_->setValue(0);
    retainSlider_->setFixedWidth(120);
    retainSlider_->setToolTip(tr("点在屏幕上的留存时间"));
    retainLabel_ = new QLabel(tr("留存 0 ms"), this);

    // 点大小调节：半径 1 ~ 8 px
    sizeSlider_ = new QSlider(Qt::Horizontal, this);
    sizeSlider_->setRange(1, 8);
    sizeSlider_->setSingleStep(1);
    sizeSlider_->setValue(2);
    sizeSlider_->setFixedWidth(80);
    sizeSlider_->setToolTip(tr("点的大小（半径 px）"));
    sizeLabel_ = new QLabel(tr("点 2 px"), this);

    toolbar->addWidget(new QLabel(tr(" 端口 "), this));
    toolbar->addWidget(portCombo_);
    toolbar->addWidget(new QLabel(tr(" 波特率 "), this));
    toolbar->addWidget(baudCombo_);
    toolbar->addWidget(startButton_);
    toolbar->addWidget(stopButton_);
    toolbar->addWidget(refreshButton);
    toolbar->addWidget(demoButton);
    toolbar->addSeparator();
    toolbar->addWidget(retainLabel_);
    toolbar->addWidget(retainSlider_);
    toolbar->addWidget(sizeLabel_);
    toolbar->addWidget(sizeSlider_);

    statusLabel_ = new QLabel(tr("未连接"), this);
    statusBar()->addWidget(statusLabel_);

    refreshPorts();

    connect(startButton_, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(demoButton, &QPushButton::clicked, this, &MainWindow::loadDemoScan);
    connect(retainSlider_, &QSlider::valueChanged, this, [this](int v) {
        view_->setRetainMs(v);
        retainLabel_->setText(tr("留存 %1 ms").arg(v));
    });
    connect(sizeSlider_, &QSlider::valueChanged, this, [this](int v) {
        view_->setPointRadius(v);
        sizeLabel_->setText(tr("点 %1 px").arg(v));
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

void MainWindow::onStartClicked() {
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
        startButton_->setEnabled(false);
        stopButton_->setEnabled(true);
        statusLabel_->setText(tr("已连接 %1 @ %2").arg(port).arg(baud));
    } else {
        statusLabel_->setText(tr("打开失败：%1").arg(transport_->errorString()));
    }
}

void MainWindow::onStopClicked() {
    transport_->close();
    startButton_->setEnabled(true);
    stopButton_->setEnabled(false);
    statusLabel_->setText(tr("已断开"));
    view_->setScan(nullptr);
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
