#pragma once

#include <QMainWindow>

class QComboBox;
class QLabel;
class QPushButton;

namespace eai {

class ScanView;
class SerialTransport;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onConnectClicked();
    void refreshPorts();

private:
    void loadDemoScan();

    ScanView* view_ = nullptr;
    SerialTransport* transport_ = nullptr;
    QComboBox* portCombo_ = nullptr;
    QComboBox* baudCombo_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

} // namespace eai
