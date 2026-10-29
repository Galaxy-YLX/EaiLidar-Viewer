#pragma once

#include <QMainWindow>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace eai {

class ScanView;
class SerialTransport;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onPowerOnClicked();
    void onPowerOffClicked();
    void refreshPorts();

private:
    void loadDemoScan();

    ScanView* view_ = nullptr;
    SerialTransport* transport_ = nullptr;
    QComboBox* portCombo_ = nullptr;
    QComboBox* baudCombo_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPushButton* disconnectButton_ = nullptr;
    QPushButton* powerOnButton_ = nullptr;
    QPushButton* powerOffButton_ = nullptr;
    QSpinBox* retainSpin_ = nullptr;
    QDoubleSpinBox* sizeSpin_ = nullptr;
    QComboBox* rangeCombo_ = nullptr;
    QComboBox* mirrorCombo_ = nullptr;
    QComboBox* rotationCombo_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

} // namespace eai
