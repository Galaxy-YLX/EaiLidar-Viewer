#include "app/mainwindow.h"

#include <cmath>
#include <memory>

#include <QComboBox>
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
    auto* refreshButton = new QPushButton(tr("刷新"), this);
    auto* demoButton = new QPushButton(tr("演示"), this);

    toolbar->addWidget(new QLabel(tr(" 端口 "), this));
    toolbar->addWidget(portCombo_);
    toolbar->addWidget(new QLabel(tr(" 波特率 "), this));
    toolbar->addWidget(baudCombo_);
    toolbar->addWidget(connectButton_);
    toolbar->addWidget(refreshButton);
    toolbar->addWidget(demoButton);

    statusLabel_ = new QLabel(tr("未连接"), this);
    statusBar()->addWidget(statusLabel_);

    refreshPorts();

    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(demoButton, &QPushButton::clicked, this, &MainWindow::loadDemoScan);

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
        transport_->close();
        connectButton_->setText(tr("连接"));
        statusLabel_->setText(tr("已断开"));
        view_->setScan(nullptr);
        return;
    }

    const QString port = portCombo_->currentText().trimmed();
    if (port.isEmpty()) {
        statusLabel_->setText(tr("请先选择串口"));
        return;
    }
    const int baud = baudCombo_->currentData().toInt();
    if (transport_->open(port, baud)) {
        connectButton_->setText(tr("断开"));
        statusLabel_->setText(tr("已连接 %1 @ %2").arg(port).arg(baud));
    } else {
        statusLabel_->setText(tr("打开失败：%1").arg(transport_->errorString()));
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
