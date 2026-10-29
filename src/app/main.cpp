#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

#include "app/mainwindow.h"

namespace {

// 绿黑「雷达」主题：纯黑底 + 绿边框绿字（与点云同款绿 #00DC78）。
// Fusion 风格 + 调色板兜底，QSS 覆盖按钮/输入框等关键控件，保证无其他底色。
void applyRadarTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));

    const QColor black(0, 0, 0);         // 统一纯黑底
    const QColor green(0, 220, 120);     // 前景绿（与点色一致）
    const QColor greenDim(0, 90, 50);    // 禁用态文字/边框

    QPalette pal;
    pal.setColor(QPalette::Window, black);
    pal.setColor(QPalette::WindowText, green);
    pal.setColor(QPalette::Base, black);
    pal.setColor(QPalette::AlternateBase, black);
    pal.setColor(QPalette::Text, green);
    pal.setColor(QPalette::Button, black);
    pal.setColor(QPalette::ButtonText, green);
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Highlight, green);
    pal.setColor(QPalette::HighlightedText, black);
    pal.setColor(QPalette::ToolTipBase, black);
    pal.setColor(QPalette::ToolTipText, green);
    pal.setColor(QPalette::Link, green);
    pal.setColor(QPalette::PlaceholderText, greenDim);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, greenDim);
    pal.setColor(QPalette::Disabled, QPalette::Text, greenDim);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, greenDim);
    app.setPalette(pal);

    app.setStyleSheet(QStringLiteral(R"(
QMainWindow, QWidget {
    background-color: #000000;
    color: #00DC78;
}
QToolBar {
    background-color: #000000;
    border: none;
    spacing: 4px;
}
QStatusBar {
    background-color: #000000;
    border-top: 1px solid #00DC78;
    color: #00DC78;
}
QPushButton {
    background-color: #000000;
    color: #00DC78;
    border: 1px solid #00DC78;
    border-radius: 2px;
    padding: 4px 12px;
    min-height: 40px;
    max-height: 40px;
}
QPushButton:hover {
    background-color: #00DC78;
    color: #000000;
}
QPushButton:pressed {
    background-color: #00a85a;
    color: #000000;
}
QPushButton:disabled {
    color: #005A32;
    border-color: #005A32;
    background-color: #000000;
}
QCheckBox {
    background-color: #000000;
    color: #00DC78;
    spacing: 8px;
    border: 1px solid #00DC78;  /* 整个按键一个边框 */
    border-radius: 2px;
    padding: 6px 10px;          /* 留出内边距，方块在框内 */
    outline: none;
    min-height: 40px;
    max-height: 40px;
}
QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: none;  /* 方块无边框，位于文字框内部左侧 */
    background-color: #000000;
}
QCheckBox::indicator:checked {
    background-color: #00DC78;  /* 按下启用：方块亮起 */
}
QCheckBox:disabled {
    color: #005A32;
    border-color: #005A32;
}
QCheckBox::indicator:disabled {
    background-color: #000000;
}
QComboBox, QSpinBox, QDoubleSpinBox {
    background-color: #000000;
    color: #00DC78;
    border: 1px solid #00DC78;
    border-radius: 2px;
    padding: 2px 6px;
    min-height: 40px;
    max-height: 40px;
}
QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover {
    border-color: #00DC78;
}
QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
    color: #005A32;
    border-color: #005A32;
}
QComboBox::drop-down {
    border: none;
    width: 18px;
}
QComboBox::down-arrow {
    image: none;
    width: 0;
    height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 6px solid #00DC78;
    margin-right: 5px;
}
QComboBox QAbstractItemView {
    background-color: #000000;
    color: #00DC78;
    border: 1px solid #00DC78;
    selection-background-color: #00DC78;
    selection-color: #000000;
    outline: none;
}
QComboBox QLineEdit {
    background-color: #000000;
    color: #00DC78;
    border: none;
    padding: 1px;
    selection-background-color: #00DC78;
    selection-color: #000000;
}
QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
    background-color: #000000;
    border: none;
    width: 14px;
}
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
    image: none;
    width: 0;
    height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-bottom: 6px solid #00DC78;
}
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
    image: none;
    width: 0;
    height: 0;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 6px solid #00DC78;
}
QToolTip {
    background-color: #000000;
    color: #00DC78;
    border: 1px solid #00DC78;
    padding: 2px 6px;
}
QMenu {
    background-color: #000000;
    color: #00DC78;
    border: 1px solid #00DC78;
}
QMenu::item:selected {
    background-color: #00DC78;
    color: #000000;
}
)"));
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
