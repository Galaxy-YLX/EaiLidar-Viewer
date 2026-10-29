#pragma once

#include <QMainWindow>

namespace eai {

class ScanView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    ScanView* view_ = nullptr;
};

} // namespace eai
