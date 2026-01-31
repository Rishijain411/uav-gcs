#include "ui/MainWindow_QGC.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    
    // Update timer for simulated data
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateDisplay);
    updateTimer->start(200); // 5Hz
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI() {
    setWindowTitle("GCS-Vyuha Control Station");
    setMinimumSize(1200, 800);
    
    // Dark theme
    setStyleSheet(
        "QMainWindow { background-color: #2b2b2b; }"
        "QLabel { color: #e0e0e0; }"
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #444; }"
    );
    
    // Create main sections
    createTopToolbar();
    createCenterLayout();
    createStatusBar();
}

void MainWindow::createTopToolbar() {
    QToolBar* toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);
    toolbar->setStyleSheet(
        "QToolBar { background-color: #3c3c3c; border-bottom: 2px solid #555; spacing: 20px; padding: 8px; }"
        "QLabel { font-size: 12pt; font-weight: bold; padding: 0 15px; }"
    );
    
    // Connection status
    QLabel* lblConnLabel = new QLabel("STATUS:");
    lblConnection = new QLabel("DISCONNECTED");
    lblConnection->setStyleSheet("QLabel { color: #ff4444; }");
    
    // Armed status
    QLabel* lblArmLabel = new QLabel("ARMED:");
    lblArmed = new QLabel("DISARMED");
    lblArmed->setStyleSheet("QLabel { color: #888; }");
    
    // Flight mode
    QLabel* lblModeLabel = new QLabel("MODE:");
    lblFlightMode = new QLabel("UNKNOWN");
    lblFlightMode->setStyleSheet("QLabel { color: #888; }");
    
    // Mission status
    QLabel* lblMissionLabel = new QLabel("MISSION:");
    lblMissionStatus = new QLabel("IDLE");
    lblMissionStatus->setStyleSheet("QLabel { color: #4CAF50; }");
    
    toolbar->addWidget(lblConnLabel);
    toolbar->addWidget(lblConnection);
    toolbar->addSeparator();
    toolbar->addWidget(lblArmLabel);
    toolbar->addWidget(lblArmed);
    toolbar->addSeparator();
    toolbar->addWidget(lblModeLabel);
    toolbar->addWidget(lblFlightMode);
    toolbar->addSeparator();
    toolbar->addWidget(lblMissionLabel);
    toolbar->addWidget(lblMissionStatus);
}

void MainWindow::createCenterLayout() {
    QWidget* centralWidget = new QWidget();
    setCentralWidget(centralWidget);
    
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // LEFT: Instrument panel
    QWidget* leftPanel = new QWidget();
    leftPanel->setFixedWidth(250);
    leftPanel->setStyleSheet("QWidget { background-color: #333; border-right: 2px solid #555; }");
    createInstrumentPanel(leftPanel);
    
    // CENTER: Map/Mission view
    QWidget* centerPanel = new QWidget();
    centerPanel->setStyleSheet("QWidget { background-color: #2b2b2b; }");
    createMapPanel(centerPanel);
    
    // RIGHT: Action panel
    QWidget* rightPanel = new QWidget();
    rightPanel->setFixedWidth(220);
    rightPanel->setStyleSheet("QWidget { background-color: #333; border-left: 2px solid #555; }");
    createActionPanel(rightPanel);
    
    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(centerPanel, 1);
    mainLayout->addWidget(rightPanel);
}

void MainWindow::createInstrumentPanel(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(15, 20, 15, 20);
    layout->setSpacing(20);
    
    QLabel* title = new QLabel("INSTRUMENTS");
    title->setStyleSheet("QLabel { font-size: 14pt; font-weight: bold; color: #4CAF50; }");
    layout->addWidget(title);
    
    layout->addSpacing(10);
    
    // Altitude
    QFrame* altFrame = new QFrame();
    QVBoxLayout* altLayout = new QVBoxLayout(altFrame);
    altLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* lblAlt = new QLabel("ALTITUDE");
    lblAlt->setStyleSheet("QLabel { font-size: 9pt; color: #888; }");
    lblAltValue = new QLabel("0.0 m");
    lblAltValue->setStyleSheet("QLabel { font-size: 20pt; font-weight: bold; color: #4CAF50; }");
    altLayout->addWidget(lblAlt);
    altLayout->addWidget(lblAltValue);
    layout->addWidget(altFrame);
    
    // Speed
    QFrame* spdFrame = new QFrame();
    QVBoxLayout* spdLayout = new QVBoxLayout(spdFrame);
    spdLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* lblSpd = new QLabel("SPEED");
    lblSpd->setStyleSheet("QLabel { font-size: 9pt; color: #888; }");
    lblSpeedValue = new QLabel("0.0 m/s");
    lblSpeedValue->setStyleSheet("QLabel { font-size: 16pt; font-weight: bold; }");
    spdLayout->addWidget(lblSpd);
    spdLayout->addWidget(lblSpeedValue);
    layout->addWidget(spdFrame);
    
    // Battery
    QFrame* batFrame = new QFrame();
    QVBoxLayout* batLayout = new QVBoxLayout(batFrame);
    batLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* lblBat = new QLabel("BATTERY");
    lblBat->setStyleSheet("QLabel { font-size: 9pt; color: #888; }");
    lblBatteryPercent = new QLabel("N/A");
    lblBatteryPercent->setStyleSheet("QLabel { font-size: 16pt; font-weight: bold; }");
    batteryBar = new QProgressBar();
    batteryBar->setRange(0, 100);
    batteryBar->setValue(0);
    batteryBar->setTextVisible(false);
    batteryBar->setStyleSheet(
        "QProgressBar { border: 1px solid #555; background-color: #222; height: 8px; }"
        "QProgressBar::chunk { background-color: #4CAF50; }"
    );
    batLayout->addWidget(lblBat);
    batLayout->addWidget(lblBatteryPercent);
    batLayout->addWidget(batteryBar);
    layout->addWidget(batFrame);
    
    // GPS
    QFrame* gpsFrame = new QFrame();
    QVBoxLayout* gpsLayout = new QVBoxLayout(gpsFrame);
    gpsLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* lblGps = new QLabel("GPS");
    lblGps->setStyleSheet("QLabel { font-size: 9pt; color: #888; }");
    lblGpsValue = new QLabel("No Fix");
    lblGpsValue->setStyleSheet("QLabel { font-size: 11pt; }");
    lblLatValue = new QLabel("Lat: 0.0°");
    lblLatValue->setStyleSheet("QLabel { font-size: 9pt; color: #aaa; }");
    lblLonValue = new QLabel("Lon: 0.0°");
    lblLonValue->setStyleSheet("QLabel { font-size: 9pt; color: #aaa; }");
    gpsLayout->addWidget(lblGps);
    gpsLayout->addWidget(lblGpsValue);
    gpsLayout->addWidget(lblLatValue);
    gpsLayout->addWidget(lblLonValue);
    layout->addWidget(gpsFrame);
    
    layout->addStretch();
}

void MainWindow::createMapPanel(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(20, 20, 20, 20);
    
    // Mission state
    lblMissionState = new QLabel("Mission: IDLE");
    lblMissionState->setStyleSheet("QLabel { font-size: 14pt; font-weight: bold; padding: 10px; background-color: #3c3c3c; border-radius: 5px; }");
    layout->addWidget(lblMissionState);
    
    // Mission log/map area
    QLabel* mapLabel = new QLabel("[ MAP VIEW ]");
    mapLabel->setAlignment(Qt::AlignCenter);
    mapLabel->setStyleSheet(
        "QLabel { font-size: 18pt; color: #666; border: 2px dashed #444; padding: 100px; background-color: #222; }"
    );
    layout->addWidget(mapLabel, 1);
    
    // Authorization panel
    QFrame* authFrame = new QFrame();
    authFrame->setStyleSheet("QFrame { background-color: #3c3c3c; border-radius: 5px; padding: 15px; }");
    QVBoxLayout* authLayout = new QVBoxLayout(authFrame);
    
    QLabel* authTitle = new QLabel("AUTHORIZATION");
    authTitle->setStyleSheet("QLabel { font-size: 11pt; font-weight: bold; color: #FFA726; }");
    authLayout->addWidget(authTitle);
    
    QLabel* authMsg = new QLabel("No pending authorization");
    authMsg->setStyleSheet("QLabel { color: #aaa; padding: 5px 0; }");
    authLayout->addWidget(authMsg);
    
    QHBoxLayout* authBtnLayout = new QHBoxLayout();
    btnApprove = new QPushButton("✓ APPROVE");
    btnApprove->setEnabled(false);
    btnApprove->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 12px 20px; border-radius: 4px; }"
        "QPushButton:disabled { background-color: #2d5e2d; color: #666; }"
        "QPushButton:hover:!disabled { background-color: #66BB6A; }"
    );
    connect(btnApprove, &QPushButton::clicked, this, &MainWindow::onApproveClicked);
    
    btnDeny = new QPushButton("✗ DENY");
    btnDeny->setEnabled(false);
    btnDeny->setStyleSheet(
        "QPushButton { background-color: #f44336; color: white; font-weight: bold; padding: 12px 20px; border-radius: 4px; }"
        "QPushButton:disabled { background-color: #5e2d2d; color: #666; }"
        "QPushButton:hover:!disabled { background-color: #EF5350; }"
    );
    connect(btnDeny, &QPushButton::clicked, this, &MainWindow::onDenyClicked);
    
    authBtnLayout->addWidget(btnApprove);
    authBtnLayout->addWidget(btnDeny);
    authLayout->addLayout(authBtnLayout);
    
    layout->addWidget(authFrame);
    
    // Mission log
    txtMissionLog = new QTextEdit();
    txtMissionLog->setReadOnly(true);
    txtMissionLog->setMaximumHeight(150);
    txtMissionLog->setStyleSheet(
        "QTextEdit { font-family: 'Courier New', monospace; font-size: 9pt; background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #444; }"
    );
    txtMissionLog->append("[GCS] System ready");
    layout->addWidget(txtMissionLog);
}

void MainWindow::createActionPanel(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(15, 20, 15, 20);
    layout->setSpacing(15);
    
    QLabel* title = new QLabel("ACTIONS");
    title->setStyleSheet("QLabel { font-size: 14pt; font-weight: bold; color: #4CAF50; }");
    layout->addWidget(title);
    
    layout->addSpacing(10);
    
    // ARM button
    btnArm = new QPushButton("ARM");
    btnArm->setMinimumHeight(50);
    btnArm->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; font-size: 12pt; font-weight: bold; border-radius: 6px; }"
        "QPushButton:hover { background-color: #66BB6A; }"
        "QPushButton:pressed { background-color: #388E3C; }"
    );
    connect(btnArm, &QPushButton::clicked, this, &MainWindow::onArmClicked);
    layout->addWidget(btnArm);
    
    // DISARM button
    btnDisarm = new QPushButton("DISARM");
    btnDisarm->setMinimumHeight(50);
    btnDisarm->setStyleSheet(
        "QPushButton { background-color: #f44336; color: white; font-size: 12pt; font-weight: bold; border-radius: 6px; }"
        "QPushButton:hover { background-color: #EF5350; }"
        "QPushButton:pressed { background-color: #D32F2F; }"
    );
    connect(btnDisarm, &QPushButton::clicked, this, &MainWindow::onDisarmClicked);
    layout->addWidget(btnDisarm);
    
    layout->addSpacing(10);
    
    // TAKEOFF button
    btnTakeoff = new QPushButton("TAKEOFF");
    btnTakeoff->setMinimumHeight(45);
    btnTakeoff->setStyleSheet(
        "QPushButton { background-color: #2196F3; color: white; font-size: 11pt; font-weight: bold; border-radius: 6px; }"
        "QPushButton:hover { background-color: #42A5F5; }"
        "QPushButton:pressed { background-color: #1976D2; }"
    );
    connect(btnTakeoff, &QPushButton::clicked, this, &MainWindow::onTakeoffClicked);
    layout->addWidget(btnTakeoff);
    
    // LAND button
    btnLand = new QPushButton("LAND");
    btnLand->setMinimumHeight(45);
    btnLand->setStyleSheet(
        "QPushButton { background-color: #FF9800; color: white; font-size: 11pt; font-weight: bold; border-radius: 6px; }"
        "QPushButton:hover { background-color: #FFA726; }"
        "QPushButton:pressed { background-color: #F57C00; }"
    );
    connect(btnLand, &QPushButton::clicked, this, &MainWindow::onLandClicked);
    layout->addWidget(btnLand);
    
    layout->addSpacing(10);
    
    // ABORT button
    btnAbort = new QPushButton("ABORT\nMISSION");
    btnAbort->setMinimumHeight(60);
    btnAbort->setStyleSheet(
        "QPushButton { background-color: #9C27B0; color: white; font-size: 11pt; font-weight: bold; border-radius: 6px; }"
        "QPushButton:hover { background-color: #AB47BC; }"
        "QPushButton:pressed { background-color: #7B1FA2; }"
    );
    connect(btnAbort, &QPushButton::clicked, this, &MainWindow::onAbortClicked);
    layout->addWidget(btnAbort);
    
    layout->addStretch();
}

void MainWindow::createStatusBar() {
    QStatusBar* status = statusBar();
    status->setStyleSheet("QStatusBar { background-color: #3c3c3c; color: #aaa; border-top: 2px solid #555; }");
    status->showMessage("GCS-Vyuha v1.0 | Ready");
}

void MainWindow::updateDisplay() {
    // Simulated data updates - will be replaced with real telemetry
    static int counter = 0;
    counter++;
    
    // Simulate connection after 2 seconds
    if (counter > 10) {
        lblConnection->setText("CONNECTED");
        lblConnection->setStyleSheet("QLabel { color: #4CAF50; }");
    }
}

// === COMMAND HANDLERS ===
void MainWindow::onArmClicked() {
    txtMissionLog->append("[CMD] ARM requested");
    lblArmed->setText("ARMED");
    lblArmed->setStyleSheet("QLabel { color: #4CAF50; }");
}

void MainWindow::onDisarmClicked() {
    txtMissionLog->append("[CMD] DISARM requested");
    lblArmed->setText("DISARMED");
    lblArmed->setStyleSheet("QLabel { color: #888; }");
}

void MainWindow::onTakeoffClicked() {
    txtMissionLog->append("[CMD] TAKEOFF requested");
    lblFlightMode->setText("TAKEOFF");
    lblFlightMode->setStyleSheet("QLabel { color: #2196F3; }");
}

void MainWindow::onLandClicked() {
    txtMissionLog->append("[CMD] LAND requested");
    lblFlightMode->setText("LANDING");
    lblFlightMode->setStyleSheet("QLabel { color: #FF9800; }");
}

void MainWindow::onAbortClicked() {
    txtMissionLog->append("[CMD] ABORT MISSION requested");
    lblMissionStatus->setText("ABORTED");
    lblMissionStatus->setStyleSheet("QLabel { color: #f44336; }");
}

void MainWindow::onApproveClicked() {
    txtMissionLog->append("[AUTH] Transition APPROVED");
    btnApprove->setEnabled(false);
    btnDeny->setEnabled(false);
}

void MainWindow::onDenyClicked() {
    txtMissionLog->append("[AUTH] Transition DENIED");
    btnApprove->setEnabled(false);
    btnDeny->setEnabled(false);
}
