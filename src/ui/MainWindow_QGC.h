#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QFrame>
#include <QProgressBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void updateDisplay();
    void onArmClicked();
    void onDisarmClicked();
    void onTakeoffClicked();
    void onLandClicked();
    void onAbortClicked();
    void onApproveClicked();
    void onDenyClicked();

private:
    void setupUI();
    void createTopToolbar();
    void createCenterLayout();
    void createInstrumentPanel(QWidget* parent);
    void createMapPanel(QWidget* parent);
    void createActionPanel(QWidget* parent);
    void createStatusBar();

    // === TOP TOOLBAR ===
    QLabel* lblConnection;
    QLabel* lblArmed;
    QLabel* lblFlightMode;
    QLabel* lblMissionStatus;
    
    // === INSTRUMENT PANEL (LEFT) ===
    QLabel* lblAltValue;
    QLabel* lblSpeedValue;
    QLabel* lblBatteryPercent;
    QProgressBar* batteryBar;
    QLabel* lblGpsValue;
    QLabel* lblLatValue;
    QLabel* lblLonValue;
    
    // === MAP/MISSION CENTER ===
    QTextEdit* txtMissionLog;
    QLabel* lblMissionState;
    QPushButton* btnApprove;
    QPushButton* btnDeny;
    
    // === ACTION PANEL (RIGHT) ===
    QPushButton* btnArm;
    QPushButton* btnDisarm;
    QPushButton* btnTakeoff;
    QPushButton* btnLand;
    QPushButton* btnAbort;

    // Update timer
    QTimer* updateTimer;
};
