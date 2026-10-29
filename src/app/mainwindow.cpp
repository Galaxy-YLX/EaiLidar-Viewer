#include "app/mainwindow.h"

#include <cmath>
#include <memory>

#include "core/scan.h"
#include "ui/scan_view.h"

namespace eai {
namespace {

constexpr double kPi = 3.14159265358979323846;

// 临时演示数据：生成一圈近圆形点云，用于验证渲染；接上雷达后由串口数据替换。
std::shared_ptr<Scan> demoScan() {
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
    setCentralWidget(view_);
    setWindowTitle(tr("EaiLidar Viewer"));
    resize(800, 600);

    view_->setScan(demoScan());
}

} // namespace eai
