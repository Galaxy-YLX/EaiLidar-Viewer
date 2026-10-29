#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

#include "app/mainwindow.h"

namespace {

// 绿黑「雷达」主题：深色背景 + 绿色前景（与点云同款绿 #00DC78）。
// 用 Fusion 风格 + 全局调色板，保证下拉箭头、菜单、tooltip 等控件一致变色。
void applyRadarTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));

    const QColor bg(10, 10, 10);        // 窗口背景：深黑
    const QColor panel(5, 5, 5);        // 工具栏/状态栏：更深的黑
    const QColor base(14, 14, 14);      // 输入框底
    const QColor green(0, 220, 120);    // 前景绿（与点色一致）
    const QColor greenDim(60, 92, 68);  // 禁用态文字
    const QColor btn(22, 22, 22);       // 按钮底

    QPalette pal;
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::WindowText, green);
    pal.setColor(QPalette::Base, base);
    pal.setColor(QPalette::AlternateBase, bg);
    pal.setColor(QPalette::Text, green);
    pal.setColor(QPalette::Button, btn);
    pal.setColor(QPalette::ButtonText, green);
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Highlight, QColor(0, 220, 120, 90));
    pal.setColor(QPalette::HighlightedText, green);
    pal.setColor(QPalette::ToolTipBase, base);
    pal.setColor(QPalette::ToolTipText, green);
    pal.setColor(QPalette::Link, green);
    pal.setColor(QPalette::PlaceholderText, greenDim);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, greenDim);
    pal.setColor(QPalette::Disabled, QPalette::Text, greenDim);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, greenDim);
    app.setPalette(pal);
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("EaiLidar Viewer");
    app.setOrganizationName("EaiLidar");

    applyRadarTheme(app);

    eai::MainWindow window;
    window.show();
    return app.exec();
}
