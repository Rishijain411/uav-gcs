#include "ui/MainWindow_QGC.h"
#include "mission/MissionProfileParser.h"
#include <QFileInfo>
#include <QPixmap>
#include <QGraphicsOpacityEffect>
#include <QStackedLayout>
#include <QImage>
#include <QGraphicsDropShadowEffect>
#include <QToolTip>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <limits>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    setWindowState(windowState() | Qt::WindowMaximized);

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateDisplay);
    updateTimer->start(200);

    engageHoldTimer = new QTimer(this);
    engageHoldTimer->setInterval(50);
    connect(engageHoldTimer, &QTimer::timeout, this, [this]() {
        engageHoldMs_ += 50;
        int pct = static_cast<int>((engageHoldMs_ * 100) / engageHoldRequiredMs_);
        if (pct > 100) pct = 100;
        engageHoldProgress->setValue(pct);
        if (engageHoldMs_ >= engageHoldRequiredMs_) {
            engageHoldTimer->stop();
            btnEngageHold->setText("ENGAGE CONFIRMED");
            appendAuditLog("[AUTHORIZED] Operator confirmed engagement", "#4ADE80");
            // Send engage command to backend
            if (backend_) {
                backend_->sendEngageCommand();
            }
        }
    });

    preflightHoldTimer = new QTimer(this);
    preflightHoldTimer->setInterval(50);
    connect(preflightHoldTimer, &QTimer::timeout, this, [this]() {
        preflightHoldMs_ += 50;
        int pct = static_cast<int>((preflightHoldMs_ * 100) / preflightHoldRequiredMs_);
        if (pct > 100) pct = 100;
        preflightHoldProgress->setValue(pct);
        if (preflightHoldMs_ >= preflightHoldRequiredMs_) {
            preflightHoldTimer->stop();
            btnPreflightHold->setText("PRE-FLIGHT CONFIRMED");
            appendAuditLog("[AUTHORIZED] Pre-flight confirmed", "#4ADE80");
        }
    });

    armHoldTimer = new QTimer(this);
    armHoldTimer->setInterval(50);
    connect(armHoldTimer, &QTimer::timeout, this, [this]() {
        armHoldMs_ += 50;
        int pct = static_cast<int>((armHoldMs_ * 100) / armHoldRequiredMs_);
        if (pct > 100) pct = 100;
        if (armHoldProgress) armHoldProgress->setValue(pct);
        if (armHoldMs_ >= armHoldRequiredMs_) {
            armHoldTimer->stop();
            if (btnArm) btnArm->setText("ARM CONFIRMED");
            onArmClicked();
        }
    });

    takeoffHoldTimer = new QTimer(this);
    takeoffHoldTimer->setInterval(50);
    connect(takeoffHoldTimer, &QTimer::timeout, this, [this]() {
        takeoffHoldMs_ += 50;
        int pct = static_cast<int>((takeoffHoldMs_ * 100) / takeoffHoldRequiredMs_);
        if (pct > 100) pct = 100;
        if (takeoffHoldProgress) takeoffHoldProgress->setValue(pct);
        if (takeoffHoldMs_ >= takeoffHoldRequiredMs_) {
            takeoffHoldTimer->stop();
            if (btnTakeoff) btnTakeoff->setText("TAKEOFF CONFIRMED");
            if (takeoffHoldProgress) {
                takeoffHoldProgress->setValue(0);  // Reset to 0
                takeoffHoldProgress->hide();        // Hide the green bar
            }
            onTakeoffClicked();
        }
    });

    abortHoldTimer = new QTimer(this);
    abortHoldTimer->setInterval(50);
    connect(abortHoldTimer, &QTimer::timeout, this, [this]() {
        abortHoldMs_ += 50;
        int pct = static_cast<int>((abortHoldMs_ * 100) / abortHoldRequiredMs_);
        if (pct > 100) pct = 100;
        if (abortHoldProgress) abortHoldProgress->setValue(pct);
        if (abortHoldMs_ >= abortHoldRequiredMs_) {
            abortHoldTimer->stop();
            if (btnAbort) btnAbort->setText("ABORT CONFIRMED");
            onAbortClicked();
        }
    });
}

MainWindow::~MainWindow() = default;

static uint32_t missionChecksum(const mission::MissionProfile& profile) {
    uint32_t hash = 2166136261u;
    auto mix = [&hash](uint32_t v) {
        hash ^= v;
        hash *= 16777619u;
    };

    mix(static_cast<uint32_t>(profile.schema_version));
    mix(static_cast<uint32_t>(profile.search_area.vertices.size()));
    mix(static_cast<uint32_t>(profile.waypoints.size()));
    for (const auto& wp : profile.waypoints) {
        mix(static_cast<uint32_t>(wp.lat * 1e7));
        mix(static_cast<uint32_t>(wp.lon * 1e7));
        mix(static_cast<uint32_t>(wp.alt * 100));
    }
    return hash;
}

static QPixmap loadLogoPixmap(int width, int height) {
    const QStringList candidates = {
        "docs/Screenshot from 2026-02-08 19-01-47.png",
        "../docs/Screenshot from 2026-02-08 19-01-47.png",
        "../../docs/Screenshot from 2026-02-08 19-01-47.png"
    };
    for (const auto& path : candidates) {
        QFileInfo info(path);
        if (!info.exists()) {
            continue;
        }
        QPixmap pix(path);
        if (!pix.isNull()) {
            QImage img = pix.toImage().convertToFormat(QImage::Format_ARGB32);
            const QColor darkInk(17, 24, 39);
            for (int y = 0; y < img.height(); ++y) {
                QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = 0; x < img.width(); ++x) {
                    const QColor c = QColor::fromRgb(row[x]);
                    if (c.red() < 15 && c.green() < 15 && c.blue() < 15) {
                        row[x] = qRgba(c.red(), c.green(), c.blue(), 0);
                    } else if (c.red() > 230 && c.green() > 230 && c.blue() > 230) {
                        row[x] = qRgba(darkInk.red(), darkInk.green(), darkInk.blue(), 255);
                    }
                }
            }
            QPixmap cleaned = QPixmap::fromImage(img);
            return cleaned.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    return QPixmap();
}

void MainWindow::setupUI() {
    setWindowTitle("GCS-Vyuha | Autonomous Interceptor C2");
    setMinimumSize(1400, 800);
    resize(1500, 850);

        setStyleSheet(
            "QMainWindow, QDialog { background-color: #F9FAFB; color: #111827; font-family: 'Inter', 'Roboto', 'Segoe UI', sans-serif; font-weight: 400; font-size: 15px; }"
            "QFrame#sidebar, QFrame#rightPanel, QFrame#centerPanel, QFrame#bottomPanel { background-color: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 16px; }"
            "QScrollArea#sidebarScroll, QWidget#sidebarContent { background-color: #FFFFFF; }"
            "QGroupBox { background-color: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 12px; margin-top: 12px; font-size: 14px; }"
            "QFrame#sidebar QLabel, QFrame#sidebar QCheckBox, QFrame#sidebar QGroupBox, QFrame#rightPanel QLabel { color: #2D3748; font-size: 14px; font-weight: 400; }"
            "QFrame#assetCard { background-color: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 16px; margin: 8px; }"
            "QLabel#phaseActive { color: #10B981; font-weight: 700; }"
            "QLabel#phaseInactive { color: #6B7280; }"
            "QTabWidget::pane { border: 1px solid #E5E7EB; border-radius: 12px; }"
            "QTabBar::tab { background: #FFFFFF; color: #2D3748; padding: 8px 16px; border: 1px solid #E5E7EB; font-size: 14px; font-weight: 500; }"
            "QTabBar::tab:selected { background: #F3F4F6; color: #111827; }"
            "QPushButton { background-color: #FFFFFF; border: 1px solid #E5E7EB; padding: 8px 16px; color: #000000; border-radius: 8px; font-size: 14px; font-weight: 500; }"
            "QPushButton:hover { background-color: #F3F4F6; }"
            "QPushButton:disabled { background-color: #E5E7EB; color: #9CA3AF; border: 1px solid #D1D5DB; }"
            "QFrame#sidebar QPushButton { background-color: #FFFFFF; border: 1px solid #E5E7EB; color: #111827; font-size: 14px; padding: 8px 16px; }"
            "QFrame#sidebar QPushButton:hover { background-color: #F3F4F6; }"
            "QPushButton#engageButton { background-color: #111827; border: 1px solid #111827; color: #FFFFFF; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 4px 10px; color: #111827; background: #F3F4F6; border-radius: 8px; font-weight: 500; font-size: 16px; }"
            "QComboBox, QSpinBox, QLineEdit { background-color: #FFFFFF; border: 1px solid #E5E7EB; color: #111827; padding: 8px 10px; border-radius: 10px; font-weight: 400; font-size: 14px; }"
            "QComboBox QAbstractItemView { background: #FFFFFF; color: #111827; selection-background-color: #E5E7EB; selection-color: #111827; }"
            "QPlainTextEdit#auditLog, QTextEdit#auditLog { background-color: #FFFFFF; border: 1px solid #E5E7EB; color: #111827; font-family: 'Roboto Mono','JetBrains Mono','Consolas', monospace; font-size: 13px; line-height: 1.5; }"
            "QCheckBox::indicator { width: 14px; height: 14px; border-radius: 3px; border: 1px solid #CBD5E1; background: #FFFFFF; }"
            "QCheckBox::indicator:checked { background: #10B981; border: 1px solid #10B981; }"
            "QCheckBox::indicator:disabled { background: #E5E7EB; border: 1px solid #CBD5E1; }"
            "QProgressBar { border: 1px solid #E5E7EB; background: #F3F4F6; height: 12px; border-radius: 8px; }"
            "QProgressBar::chunk { background-color: #10B981; border-radius: 8px; }"
            "QSplitter::handle { background: #E5E7EB; }"
        );

    createTopToolbar();
    createCenterLayout();
    createStatusBar();
}

void MainWindow::createTopToolbar() {
    QToolBar* toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);
        toolbar->setStyleSheet(
            "QToolBar { background-color: #FFFFFF; border-bottom: 1px solid #E5E7EB; spacing: 12px; padding: 8px 10px; }"
            "QLabel { font-size: 16px; font-weight: 500; padding: 0 8px; color: #111827; }"
        );

    QLabel* brandLogo = new QLabel();
    const QPixmap brandPixmap = loadLogoPixmap(80, 80);
    if (!brandPixmap.isNull()) {
        brandLogo->setPixmap(brandPixmap);
        brandLogo->setFixedSize(80, 80);
        brandLogo->setScaledContents(true);
    }
    toolbar->addWidget(brandLogo);
    toolbar->addSeparator();

    QLabel* lblConnLabel = new QLabel("● LINK:");
    lblConnection = new QLabel("DISCONNECTED");
    lblConnection->setStyleSheet("QLabel { color: #10B981; }");

    QLabel* lblEncLabel = new QLabel("● SECURITY:");
    lblEncryption = new QLabel("UNSIGNED");
    lblEncryption->setStyleSheet("QLabel { color: #111827; }");

    QLabel* lblLatLabel = new QLabel("● LATENCY:");
    lblLatency = new QLabel("-- ms");

    QLabel* lblRateLabel = new QLabel("● TELEM:");
    lblTelemetryRate = new QLabel("-- Hz");

    QLabel* lblCountLabel = new QLabel("● ASSETS:");
    lblActiveCount = new QLabel("0");

    QLabel* lblArmLabel = new QLabel("● ARMED:");
    lblArmed = new QLabel("DISARMED");
    lblArmed->setStyleSheet("QLabel { color: #111827; }");
    auto* armedGlow = new QGraphicsDropShadowEffect(this);
    armedGlow->setBlurRadius(10);
    armedGlow->setOffset(0, 0);
    armedGlow->setColor(QColor(16, 185, 129, 80));
    lblArmed->setGraphicsEffect(armedGlow);

    QLabel* lblModeLabel = new QLabel("● MODE:");
    lblFlightMode = new QLabel("UNKNOWN");
    lblFlightMode->setStyleSheet("QLabel { color: #111827; }");

    QLabel* lblMissionLabel = new QLabel("● MISSION:");
    lblMissionStatus = new QLabel("IDLE");
    lblMissionStatus->setStyleSheet("QLabel { color: #10B981; }");

    toolbar->addWidget(lblConnLabel);
    toolbar->addWidget(lblConnection);
    toolbar->addSeparator();
    toolbar->addWidget(lblEncLabel);
    toolbar->addWidget(lblEncryption);
    toolbar->addSeparator();
    toolbar->addWidget(lblLatLabel);
    toolbar->addWidget(lblLatency);
    toolbar->addSeparator();
    toolbar->addWidget(lblRateLabel);
    toolbar->addWidget(lblTelemetryRate);
    toolbar->addSeparator();
    toolbar->addWidget(lblCountLabel);
    toolbar->addWidget(lblActiveCount);
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

    QVBoxLayout* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* topLayout = new QHBoxLayout();
    topLayout->setSpacing(0);
    topLayout->setContentsMargins(0, 0, 0, 0);

    QFrame* leftPanel = new QFrame();
    leftPanel->setObjectName("sidebar");
    leftPanel->setMinimumWidth(520);

    auto applyCardShadow = [](QWidget* widget) {
        auto* shadow = new QGraphicsDropShadowEffect(widget);
        shadow->setBlurRadius(12);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(0, 0, 0, 13));
        widget->setGraphicsEffect(shadow);
    };

    QScrollArea* leftScroll = new QScrollArea();
    leftScroll->setObjectName("sidebarScroll");
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    QWidget* leftContent = new QWidget();
    leftContent->setObjectName("sidebarContent");
    createLeftSidebar(leftContent);
    leftScroll->setWidget(leftContent);

    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(leftScroll);
    applyCardShadow(leftPanel);

    QWidget* centerPanel = new QFrame();
    centerPanel->setObjectName("centerPanel");
    createCenterPanel(centerPanel);
    applyCardShadow(centerPanel);

    QWidget* rightPanel = new QFrame();
    rightPanel->setObjectName("rightPanel");
    createRightPanel(rightPanel);
    applyCardShadow(rightPanel);

    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(leftPanel);
    splitter->addWidget(centerPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({520, 760, 260});

    topLayout->addWidget(splitter);

    QFrame* bottomPanel = new QFrame();
    bottomPanel->setObjectName("bottomPanel");
    bottomPanel->setFixedHeight(200);
    createBottomPanel(bottomPanel);
    applyCardShadow(bottomPanel);

    rootLayout->addLayout(topLayout, 1);
    rootLayout->addWidget(bottomPanel);
}

void MainWindow::createLeftSidebar(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    QFrame* topSpacer = new QFrame();
    topSpacer->setFixedHeight(10);
    layout->addWidget(topSpacer);

    QLabel* title = new QLabel("MISSION FLOW");
    title->setStyleSheet("QLabel { font-size: 20px; font-weight: 500; color: #111827; background: #F3F4F6; padding: 8px 12px; border-radius: 10px; }");
    layout->addWidget(title);

    QFrame* phaseBar = new QFrame();
    QHBoxLayout* phaseLayout = new QHBoxLayout(phaseBar);
    phaseLayout->setContentsMargins(0, 0, 0, 0);
    phaseLayout->setSpacing(8);

    lblPhaseInit = new QLabel("PRE-FLIGHT");
    lblPhaseInit->setObjectName("phaseActive");
    lblPhaseArm = new QLabel("ARM");
    lblPhaseArm->setObjectName("phaseInactive");
    lblPhaseSearch = new QLabel("SEARCH");
    lblPhaseSearch->setObjectName("phaseInactive");
    lblPhaseEngage = new QLabel("ENGAGE");
    lblPhaseEngage->setObjectName("phaseInactive");
    lblPhaseRtl = new QLabel("RTL");
    lblPhaseRtl->setObjectName("phaseInactive");

    phaseLayout->addWidget(lblPhaseInit);
    phaseLayout->addWidget(new QLabel(">"));
    phaseLayout->addWidget(lblPhaseArm);
    phaseLayout->addWidget(new QLabel(">"));
    phaseLayout->addWidget(lblPhaseSearch);
    phaseLayout->addWidget(new QLabel(">"));
    phaseLayout->addWidget(lblPhaseEngage);
    phaseLayout->addWidget(new QLabel(">"));
    phaseLayout->addWidget(lblPhaseRtl);
    layout->addWidget(phaseBar);

    QTabWidget* tabs = new QTabWidget();

    QWidget* tabSetup = new QWidget();
    QVBoxLayout* setupLayout = new QVBoxLayout(tabSetup);
    QPushButton* btnUpload = new QPushButton("Upload Mission Profile");
    connect(btnUpload, &QPushButton::clicked, this, &MainWindow::onMissionProfileUpload);
        lblMissionFile = new QLabel("No file loaded");
        lblMissionFile->setStyleSheet("QLabel { color: #6B7280; font-size: 16px; }");
    setupLayout->addWidget(btnUpload);
    setupLayout->addWidget(lblMissionFile);

    QGroupBox* missionSummary = new QGroupBox("Mission Summary");
    QVBoxLayout* summaryLayout = new QVBoxLayout(missionSummary);
    lblMissionSummaryTarget = new QLabel("Target: —");
    lblMissionSummaryArea = new QLabel("Search Area: —");
    lblMissionSummaryWaypoints = new QLabel("Waypoints: —");
    lblMissionSummaryAltitude = new QLabel("Altitude Band: —");
    lblMissionSummaryPayload = new QLabel("Payload: —");
    lblMissionSummaryFailsafe = new QLabel("Fail-safe: —");
    lblMissionSummaryStatus = new QLabel("Upload Status: NOT LOADED");
    lblMissionSummaryChecksum = new QLabel("Checksum: —");
    summaryLayout->addWidget(lblMissionSummaryTarget);
    summaryLayout->addWidget(lblMissionSummaryArea);
    summaryLayout->addWidget(lblMissionSummaryWaypoints);
    summaryLayout->addWidget(lblMissionSummaryAltitude);
    summaryLayout->addWidget(lblMissionSummaryPayload);
    summaryLayout->addWidget(lblMissionSummaryFailsafe);
    summaryLayout->addWidget(lblMissionSummaryStatus);
    summaryLayout->addWidget(lblMissionSummaryChecksum);
    setupLayout->addWidget(missionSummary);

    QGroupBox* advancedSetup = new QGroupBox("Failsafe & Pre-Flight Checks");
    advancedSetup->setCheckable(false);
    advancedSetup->setEnabled(true);
    QVBoxLayout* advLayout = new QVBoxLayout(advancedSetup);

    QLabel* advancedHelp = new QLabel("Configure safety behavior and review BIT status before confirming pre-flight.");
    advancedHelp->setWordWrap(true);
    advancedHelp->setStyleSheet("QLabel { color: #6B7280; font-size: 13px; }");
    advLayout->addWidget(advancedHelp);

    QLabel* failsafeTitle = new QLabel("Safety Fail-Safe Config");
    failsafeTitle->setWordWrap(true);
    failsafeTitle->setStyleSheet("QLabel { font-weight: 500; color: #111827; background: #F3F4F6; padding: 6px 10px; border-radius: 8px; font-size: 18px; }");
    advLayout->addWidget(failsafeTitle);

    cmbCommsLoss = new QComboBox();
    cmbCommsLoss->addItems({"RTL", "LAND", "HOLD", "CONTINUE"});
    cmbLowBattery = new QComboBox();
    cmbLowBattery->addItems({"RTL", "LAND", "HOLD", "CONTINUE"});
    cmbGpsJamming = new QComboBox();
    cmbGpsJamming->addItems({"RTL (Dead Reckoning)", "LOITER", "HOLD", "LAND"});
    cmbGpsJamming->setCurrentIndex(0);

    advLayout->addWidget(new QLabel("Comms Loss"));
    advLayout->addWidget(cmbCommsLoss);
    advLayout->addWidget(new QLabel("Low Battery"));
    advLayout->addWidget(cmbLowBattery);
    advLayout->addWidget(new QLabel("GPS Jamming"));
    advLayout->addWidget(cmbGpsJamming);

    QLabel* bitTitle = new QLabel("Pre-Flight BIT");
    bitTitle->setStyleSheet("QLabel { font-weight: 500; color: #111827; background: #F3F4F6; padding: 6px 10px; border-radius: 8px; font-size: 18px; }");
    advLayout->addWidget(bitTitle);

    chkMotors = new QCheckBox("Motors/ESCs");
    chkBattery = new QCheckBox("Battery Health");
    chkMavlink = new QCheckBox("MAVLink Heartbeat");
    chkPayload = new QCheckBox("Payload Continuity");
    chkMotors->setEnabled(true);
    chkBattery->setEnabled(true);
    chkMavlink->setEnabled(true);
    chkPayload->setEnabled(true);
    advLayout->addWidget(chkMotors);
    advLayout->addWidget(chkBattery);
    advLayout->addWidget(chkMavlink);
    advLayout->addWidget(chkPayload);

    setupLayout->addWidget(advancedSetup);

    btnPreflightHold = new QPushButton("HOLD TO CONFIRM PRE-FLIGHT");
    connect(btnPreflightHold, &QPushButton::pressed, this, &MainWindow::onPreflightPress);
    connect(btnPreflightHold, &QPushButton::released, this, &MainWindow::onPreflightRelease);
    preflightHoldProgress = new QProgressBar();
    preflightHoldProgress->setRange(0, 100);
    preflightHoldProgress->setValue(0);
    preflightHoldProgress->setTextVisible(false);
    setupLayout->addWidget(btnPreflightHold);
    setupLayout->addWidget(preflightHoldProgress);
    setupLayout->addStretch();

    QWidget* tabEngage = new QWidget();
    QVBoxLayout* engageLayout = new QVBoxLayout(tabEngage);
    cmbSearchMode = new QComboBox();
    cmbSearchMode->addItems({"Expanding Square", "Sector Search", "Loiter"});
    engageLayout->addWidget(new QLabel("Search Mode"));
    engageLayout->addWidget(cmbSearchMode);

    QLabel* oitlLabel = new QLabel("OITL Alerts: None");
    oitlLabel->setStyleSheet("QLabel { color: #6B7280; font-size: 16px; }");
    engageLayout->addWidget(oitlLabel);

    btnEngageHold = new QPushButton("PRESS & HOLD TO ENGAGE");
    btnEngageHold->setObjectName("engageButton");
    connect(btnEngageHold, &QPushButton::pressed, this, &MainWindow::onEngagePress);
    connect(btnEngageHold, &QPushButton::released, this, &MainWindow::onEngageRelease);

    engageHoldProgress = new QProgressBar();
    engageHoldProgress->setRange(0, 100);
    engageHoldProgress->setValue(0);
    engageHoldProgress->setTextVisible(false);

    spnReengageCount = new QSpinBox();
    spnReengageCount->setRange(0, 5);
    spnReengageCount->setValue(0);
    engageLayout->addWidget(btnEngageHold);
    engageLayout->addWidget(engageHoldProgress);
    engageLayout->addWidget(new QLabel("Re-Engagement Count"));
    engageLayout->addWidget(spnReengageCount);
    engageLayout->addStretch();

    QWidget* tabRecovery = new QWidget();
    QVBoxLayout* recoveryLayout = new QVBoxLayout(tabRecovery);
    lblBdaStatus = new QLabel("BDA: UNKNOWN");
    btnRtl = new QPushButton("Return To Base");
    btnLand = new QPushButton("Emergency Land");
    connect(btnRtl, &QPushButton::clicked, this, &MainWindow::onRtlClicked);
    connect(btnLand, &QPushButton::clicked, this, &MainWindow::onLandClicked);
    recoveryLayout->addWidget(lblBdaStatus);
    recoveryLayout->addWidget(btnRtl);
    recoveryLayout->addWidget(btnLand);

    QLabel* postStrike = new QLabel("Post-Strike Checklist");
    postStrike->setStyleSheet("QLabel { font-weight: 500; color: #111827; background: #F3F4F6; padding: 6px 10px; border-radius: 8px; font-size: 18px; }");
    chkBdaMotors = new QCheckBox("Motors: OK");
    chkBdaLink = new QCheckBox("Link: DEGRADED");
    chkBdaPayload = new QCheckBox("Payload: EXPENDED");
    recoveryLayout->addWidget(postStrike);
    recoveryLayout->addWidget(chkBdaMotors);
    recoveryLayout->addWidget(chkBdaLink);
    recoveryLayout->addWidget(chkBdaPayload);

    QLabel* eff = new QLabel("Target Neutralized?");
    eff->setStyleSheet("QLabel { font-weight: 500; color: #111827; background: #F3F4F6; padding: 6px 10px; border-radius: 8px; font-size: 18px; }");
    QHBoxLayout* bdaBtns = new QHBoxLayout();
    btnBdaYes = new QPushButton("YES");
    btnBdaNo = new QPushButton("NO");
    btnBdaUnknown = new QPushButton("UNKNOWN");
    connect(btnBdaYes, &QPushButton::clicked, this, &MainWindow::onBdaYes);
    connect(btnBdaNo, &QPushButton::clicked, this, &MainWindow::onBdaNo);
    connect(btnBdaUnknown, &QPushButton::clicked, this, &MainWindow::onBdaUnknown);
    bdaBtns->addWidget(btnBdaYes);
    bdaBtns->addWidget(btnBdaNo);
    bdaBtns->addWidget(btnBdaUnknown);
    recoveryLayout->addWidget(eff);
    recoveryLayout->addStretch();

    tabs->addTab(tabSetup, "Setup");
    tabs->addTab(tabEngage, "Engage");
    tabs->addTab(tabRecovery, "Recovery");
    layout->addWidget(tabs, 1);

}

void MainWindow::createCenterPanel(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    lblEngageBanner = new QLabel("ENGAGEMENT MODE ACTIVE");
    lblEngageBanner->setAlignment(Qt::AlignCenter);
    lblEngageBanner->setStyleSheet("QLabel { background-color: #111827; color: #FFFFFF; font-weight: 600; padding: 8px; border-radius: 12px; font-size: 26px; }");
    auto* engageGlow = new QGraphicsDropShadowEffect(this);
    engageGlow->setBlurRadius(12);
    engageGlow->setOffset(0, 0);
    engageGlow->setColor(QColor(16, 185, 129, 60));
    lblEngageBanner->setGraphicsEffect(engageGlow);
    lblEngageBanner->setVisible(false);
    layout->addWidget(lblEngageBanner);

    lblMissionState = new QLabel("Mission: IDLE");
    lblMissionState->setStyleSheet("QLabel { font-size: 20px; font-weight: 500; padding: 14px 16px; background-color: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 12px; }");
    layout->addWidget(lblMissionState);

    QFrame* mapFrame = new QFrame();
    mapFrame->setMinimumHeight(420);
    mapFrame->setStyleSheet("QFrame { background-color: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 16px; }");
    mapFrame_ = mapFrame;
    QStackedLayout* mapStack = new QStackedLayout(mapFrame);
    mapStack->setStackingMode(QStackedLayout::StackAll);

    QWidget* mapBackground = new QWidget();
    QVBoxLayout* mapBgLayout = new QVBoxLayout(mapBackground);
    mapBgLayout->setContentsMargins(0, 0, 0, 0);
    mapBgLayout->addStretch();
    QLabel* mapWatermark = new QLabel();
    const QPixmap mapLogo = loadLogoPixmap(300, 300);
    if (!mapLogo.isNull()) {
        mapWatermark->setPixmap(mapLogo);
        mapWatermark->setAlignment(Qt::AlignCenter);
        mapWatermark->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* mapOpacity = new QGraphicsOpacityEffect(mapWatermark);
        mapOpacity->setOpacity(0.18);
        mapWatermark->setGraphicsEffect(mapOpacity);
    }
    mapBgLayout->addWidget(mapWatermark, 0, Qt::AlignCenter);
    mapBgLayout->addStretch();

    QWidget* mapContent = new QWidget();
    QVBoxLayout* mapLayout = new QVBoxLayout(mapContent);

    lblMapPlaceholder = new QLabel("[ TACTICAL MAP ]\nCombat Bubble | Interceptor | Target | No-Fly Zones");
    lblMapPlaceholder->setAlignment(Qt::AlignCenter);
    lblMapPlaceholder->setStyleSheet("QLabel { color: #6B7280; font-size: 16px; }");
    lblMapPlaceholder->setText("[ TACTICAL MAP ]\nNFZ: ACTIVE | LOCK: ARMED | TRACK: LIVE");

    lblLockOverlay = new QLabel("LOCK ENVELOPE");
    lblLockOverlay->setAlignment(Qt::AlignCenter);
    lblLockOverlay->setFixedSize(180, 180);
    lblLockOverlay->setStyleSheet("QLabel { color: #111827; border: 2px dashed #111827; border-radius: 80px; }");

    lblCombatBubble = new QLabel("Combat Bubble: 1.5 km");
    lblCombatBubble->setStyleSheet("QLabel { color: #111827; font-weight: 500; font-size: 18px; }");
    lblTargetLock = new QLabel("Target Lock: NONE");
    lblTargetLock->setStyleSheet("QLabel { color: #6B7280; font-size: 16px; }");
    lblMapLegend = new QLabel("Legend: NFZ | Search Path | Lock Envelope");
    lblMapLegend->setStyleSheet("QLabel { color: #6B7280; font-size: 15px; }");

    mapLayout->addWidget(lblMapPlaceholder, 1);
    mapLayout->addWidget(lblLockOverlay, 0, Qt::AlignCenter);
    mapLayout->addWidget(lblCombatBubble);
    mapLayout->addWidget(lblTargetLock);
    mapLayout->addWidget(lblMapLegend);

    mapStack->addWidget(mapBackground);
    mapStack->addWidget(mapContent);
    QSplitter* centerSplitter = new QSplitter(Qt::Vertical);
    centerSplitter->setChildrenCollapsible(false);
    centerSplitter->addWidget(mapFrame);

    QGroupBox* authBox = new QGroupBox("Command Authorization");
    QVBoxLayout* authLayout = new QVBoxLayout(authBox);
    QLabel* authInfo = new QLabel("Explicit confirmation required for ARM/TAKEOFF/ABORT");
    authInfo->setStyleSheet("QLabel { color: #111827; font-size: 14px; }");
    authLayout->addWidget(authInfo);

    QHBoxLayout* primaryRow = new QHBoxLayout();
    btnArm = new QPushButton("ARM");
    btnDisarm = new QPushButton("DISARM");
    btnTakeoff = new QPushButton("TAKEOFF");
    btnAbort = new QPushButton("ABORT");
    btnAbort->setMinimumHeight(44);
    btnAbort->setMinimumWidth(120);
    connect(btnArm, &QPushButton::pressed, this, &MainWindow::onArmHoldPress);
    connect(btnArm, &QPushButton::released, this, &MainWindow::onArmHoldRelease);
    connect(btnDisarm, &QPushButton::clicked, this, &MainWindow::onDisarmClicked);
    connect(btnTakeoff, &QPushButton::pressed, this, &MainWindow::onTakeoffHoldPress);
    connect(btnTakeoff, &QPushButton::released, this, &MainWindow::onTakeoffHoldRelease);
    connect(btnAbort, &QPushButton::pressed, this, &MainWindow::onAbortHoldPress);
    connect(btnAbort, &QPushButton::released, this, &MainWindow::onAbortHoldRelease);

    primaryRow->addWidget(btnArm);
    armHoldProgress = new QProgressBar();
    armHoldProgress->setRange(0, 100);
    armHoldProgress->setValue(0);
    armHoldProgress->setTextVisible(false);
    primaryRow->addWidget(armHoldProgress);
    primaryRow->addWidget(btnDisarm);
    primaryRow->addWidget(btnTakeoff);

    takeoffHoldProgress = new QProgressBar();
    takeoffHoldProgress->setRange(0, 100);
    takeoffHoldProgress->setValue(0);
    takeoffHoldProgress->setTextVisible(false);
    primaryRow->addWidget(takeoffHoldProgress);

    authLayout->addLayout(primaryRow);

    QFrame* deadZone = new QFrame();
    deadZone->setFixedHeight(12);
    authLayout->addWidget(deadZone);

    QVBoxLayout* abortRow = new QVBoxLayout();
    abortRow->addWidget(btnAbort, 0, Qt::AlignHCenter);

    abortHoldProgress = new QProgressBar();
    abortHoldProgress->setRange(0, 100);
    abortHoldProgress->setValue(0);
    abortHoldProgress->setTextVisible(false);
    abortRow->addWidget(abortHoldProgress);

    authLayout->addLayout(abortRow);
    centerSplitter->addWidget(authBox);
    centerSplitter->setStretchFactor(0, 4);
    centerSplitter->setStretchFactor(1, 1);
    layout->addWidget(centerSplitter, 1);
}

void MainWindow::createRightPanel(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QLabel* title = new QLabel("MULTI-ASSET STATUS");
    title->setStyleSheet("QLabel { font-size: 20px; font-weight: 500; color: #111827; background: #F3F4F6; padding: 8px 12px; border-radius: 10px; }");
    layout->addWidget(title);

    QFrame* listFrame = new QFrame();
    QVBoxLayout* listLayout = new QVBoxLayout(listFrame);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(6);

    assetsLayout = listLayout;
    // Asset cards will be populated by backend signals - no hardcoded values

    layout->addWidget(listFrame, 1);
}

void MainWindow::createBottomPanel(QWidget* parent) {
    QHBoxLayout* layout = new QHBoxLayout(parent);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(12);

    QFrame* videoFrame = new QFrame();
    videoFrame->setFixedWidth(360);
    videoFrame_ = videoFrame;
    QVBoxLayout* videoLayout = new QVBoxLayout(videoFrame);
    QLabel* videoTitle = new QLabel("LOW-LATENCY VIDEO");
    videoTitle->setStyleSheet("QLabel { color: #111827; background: #F3F4F6; font-weight: 500; padding: 6px 10px; border-radius: 10px; font-size: 16px; }");
    lblVideoPlaceholder = new QLabel("[ H.264/H.265 STREAM ]");
    lblVideoPlaceholder->setAlignment(Qt::AlignCenter);
    lblVideoPlaceholder->setStyleSheet("QLabel { color: #6B7280; border: 1px dashed #E5E7EB; padding: 16px; font-size: 14px; }");
    videoLayout->addWidget(videoTitle);
    videoLayout->addWidget(lblVideoPlaceholder, 1);

    QFrame* telemetryFrame = new QFrame();
    telemetryFrame_ = telemetryFrame;
    QGridLayout* telemLayout = new QGridLayout(telemetryFrame);
    telemLayout->setContentsMargins(6, 6, 6, 6);
    telemLayout->setSpacing(6);
    QLabel* telemTitle = new QLabel("REAL-TIME METRICS");
    telemTitle->setStyleSheet("QLabel { color: #111827; background: #F3F4F6; font-weight: 500; padding: 6px 10px; border-radius: 10px; font-size: 16px; }");
    telemLayout->addWidget(telemTitle, 0, 0, 1, 2);

    lblAltValue = new QLabel("ALT: 0.0 m");
    lblSpeedValue = new QLabel("SPD: 0.0 m/s");
    lblBatteryPercent = new QLabel("BAT: --%");
    lblGpsValue = new QLabel("GPS: NO FIX");
    lblLatValue = new QLabel("LAT: --");
    lblLonValue = new QLabel("LON: --");
    const QString dataStyle = "QLabel { font-family: 'Roboto Mono','JetBrains Mono','Consolas', monospace; font-size: 16px; font-weight: 400; color: #000000; }";
    lblAltValue->setStyleSheet(dataStyle);
    lblSpeedValue->setStyleSheet(dataStyle);
    lblBatteryPercent->setStyleSheet(dataStyle);
    lblGpsValue->setStyleSheet(dataStyle);
    lblLatValue->setStyleSheet(dataStyle);
    lblLonValue->setStyleSheet(dataStyle);
    batteryBar = new QProgressBar();
    batteryBar->setRange(0, 100);
    batteryBar->setValue(0);
    batteryBar->setTextVisible(false);

    telemLayout->addWidget(lblAltValue, 1, 0);
    telemLayout->addWidget(lblSpeedValue, 1, 1);
    telemLayout->addWidget(lblBatteryPercent, 2, 0);
    telemLayout->addWidget(batteryBar, 2, 1);
    telemLayout->addWidget(lblGpsValue, 3, 0);
    telemLayout->addWidget(lblLatValue, 4, 0);
    telemLayout->addWidget(lblLonValue, 4, 1);

    QFrame* auditFrame = new QFrame();
    auditFrame_ = auditFrame;
    QVBoxLayout* auditLayout = new QVBoxLayout(auditFrame);
    QWidget* auditHeader = new QWidget();
    QHBoxLayout* auditHeaderLayout = new QHBoxLayout(auditHeader);
    auditHeaderLayout->setContentsMargins(0, 0, 0, 0);
    auditHeaderLayout->setSpacing(8);

    QLabel* auditLogo = new QLabel();
    const QPixmap auditLogoPixmap = loadLogoPixmap(20, 20);
    if (!auditLogoPixmap.isNull()) {
        auditLogo->setPixmap(auditLogoPixmap);
        auditLogo->setFixedSize(16, 16);
        auditLogo->setScaledContents(true);
        auto* auditOpacity = new QGraphicsOpacityEffect(auditLogo);
        auditOpacity->setOpacity(0.6);
        auditLogo->setGraphicsEffect(auditOpacity);
    }

    QLabel* auditTitle = new QLabel("AUDIT LOG (.tlog)");
    auditTitle->setStyleSheet("QLabel { color: #111827; background: #F3F4F6; font-weight: 500; padding: 6px 10px; border-radius: 10px; font-size: 16px; }");
    auditHeaderLayout->addWidget(auditLogo, 0, Qt::AlignVCenter);
    auditHeaderLayout->addWidget(auditTitle, 1);
    txtAuditLog = new QTextEdit();
    txtAuditLog->setObjectName("auditLog");
    txtAuditLog->setReadOnly(true);
    txtAuditLog->setMaximumHeight(190);
    auditLayout->addWidget(auditHeader);
    auditLayout->addWidget(txtAuditLog);

    QSplitter* bottomSplitter = new QSplitter(Qt::Horizontal);
    bottomSplitter->setChildrenCollapsible(false);
    bottomSplitter->addWidget(videoFrame);
    bottomSplitter->addWidget(telemetryFrame);
    bottomSplitter->addWidget(auditFrame);
    bottomSplitter->setStretchFactor(0, 0);
    bottomSplitter->setStretchFactor(1, 1);
    bottomSplitter->setStretchFactor(2, 1);
    bottomSplitter->setSizes({360, 300, 300});
    layout->addWidget(bottomSplitter, 1);
}

void MainWindow::createStatusBar() {
    QStatusBar* status = statusBar();
    status->setStyleSheet("QStatusBar { background-color: #FFFFFF; color: #6B7280; border-top: 1px solid #E5E7EB; }");
    status->showMessage("GCS-Vyuha v1.0 | Ready");
}

QFrame* MainWindow::createAssetCard(const QString& name, const QString& phase, int batteryPercent, const QString& link, const QString& urgency) {
    QFrame* card = new QFrame();
    card->setObjectName("assetCard");
    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    QLabel* lblName = new QLabel(name);
    lblName->setStyleSheet("QLabel { font-size: 18px; font-weight: 500; color: #111827; }");
    QLabel* lblPhase = new QLabel("PHASE: " + phase);
    QLabel* lblBattery = new QLabel("BATTERY: " + QString::number(batteryPercent) + "%");
    QLabel* lblLink = new QLabel("LINK: " + link);
    lblPhase->setStyleSheet("QLabel { font-size: 16px; font-weight: 400; color: #6B7280; }");
    lblBattery->setStyleSheet("QLabel { font-size: 16px; font-weight: 400; color: #6B7280; }");
    lblLink->setStyleSheet("QLabel { font-size: 16px; font-weight: 400; color: #6B7280; }");

    layout->addWidget(lblName);
    layout->addWidget(lblPhase);
    layout->addWidget(lblBattery);
    layout->addWidget(lblLink);

    const bool isActivePhase = (phase == "ENGAGE" || phase == "SEARCH");
    if (isActivePhase && batteryPercent < 50) {
        card->setStyleSheet("QFrame#assetCard { border-left: 4px solid #F59E0B; }");
        lblBattery->setStyleSheet("QLabel { font-size: 16px; font-weight: 400; color: #F59E0B; }");
    } else if (urgency == "ALERT") {
        card->setStyleSheet("QFrame#assetCard { border-left: 4px solid #111827; }");
    } else if (urgency == "LOW") {
        card->setStyleSheet("QFrame#assetCard { border-left: 4px solid #111827; }");
    } else if (urgency == "ACTIVE") {
        card->setStyleSheet("QFrame#assetCard { border-left: 4px solid #10B981; }");
    }
    return card;
}

void MainWindow::appendAuditLog(const QString& text, const QString& color) {
    if (!txtAuditLog) return;
    txtAuditLog->append("<span style='color:" + color + "'>" + text + "</span>");
}

void MainWindow::updateDisplay() {
    static int counter = 0;
    counter++;

    const bool anyEngage = (lastMissionState_ == "ENGAGE");
    const bool anySearch = (lastMissionState_ == "SEARCH" || lastMissionState_ == "TRANSIT");
    if (anyEngage) {
        if (lblMissionStatus) {
            lblMissionStatus->setText("STATUS: ENGAGE");
            lblMissionStatus->setStyleSheet("QLabel { color: #DC2626; font-weight: bold; }");
        }
        if (lblMissionState) {
            lblMissionState->setText("Mission: ENGAGING");
        }
        if (lblEngageBanner) {
            lblEngageBanner->setVisible(true);
        }
        if (lblPhaseInit) lblPhaseInit->setStyleSheet("QLabel { color: #6B7280; }");
        if (lblPhaseArm) lblPhaseArm->setStyleSheet("QLabel { color: #6B7280; }");
        if (lblPhaseSearch) lblPhaseSearch->setStyleSheet("QLabel { color: #6B7280; }");
        if (lblPhaseEngage) lblPhaseEngage->setStyleSheet("QLabel { color: #DC2626; font-weight: 700; }");
        if (lblPhaseRtl) lblPhaseRtl->setStyleSheet("QLabel { color: #6B7280; }");
        if (lblLockOverlay) {
            lblLockOverlay->setStyleSheet("QLabel { color: #111827; border: 2px solid #111827; border-radius: 80px; }");
        }
    } else if (anySearch) {
        if (lblMissionStatus) {
            lblMissionStatus->setText("STATUS: SEARCH");
            lblMissionStatus->setStyleSheet("QLabel { color: #8B5CF6; font-weight: bold; }");
        }
        if (lblMissionState) {
            lblMissionState->setText("Mission: SEARCHING");
        }
        if (lblEngageBanner) {
            lblEngageBanner->setVisible(false);
        }
        if (lblPhaseInit) lblPhaseInit->setStyleSheet("QLabel { color: #10B981; font-weight: 700; }");
        if (lblPhaseArm) lblPhaseArm->setStyleSheet("QLabel { color: #6B7280; }");
        if (lblPhaseSearch) lblPhaseSearch->setStyleSheet("QLabel { color: #8B5CF6; font-weight: 700; }");
        if (lblPhaseEngage) lblPhaseEngage->setStyleSheet("QLabel { color: #6B7280; }");
        if (lblPhaseRtl) lblPhaseRtl->setStyleSheet("QLabel { color: #6B7280; }");
        if (lblLockOverlay) {
            lblLockOverlay->setStyleSheet("QLabel { color: #111827; border: 2px dashed #111827; border-radius: 80px; }");
        }
    }


    lblCombatBubble->setText("Combat Bubble: —");
    lblTargetLock->setText("Target Lock: —");
    lblTargetLock->setStyleSheet("QLabel { color: #6B7280; }");

    // Avoid overwriting preflight/BDA checkboxes every tick.

    if (counter % 5 == 0) {
        QFile statusFile("config/mission_upload_status.json");
        if (statusFile.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(statusFile.readAll());
            const QJsonObject obj = doc.object();
            const QString status = obj.value("status").toString();
            const int ackType = obj.value("ack_type").toInt(-1);

            MissionUploadStatus newStatus = MissionUploadStatus::NOT_LOADED;
            if (status == "IN_PROGRESS") {
                newStatus = MissionUploadStatus::IN_PROGRESS;
            } else if (status == "ACCEPTED") {
                newStatus = MissionUploadStatus::ACCEPTED;
            } else if (status == "FAILED") {
                newStatus = MissionUploadStatus::FAILED;
            }

            if (newStatus != missionUploadStatus_) {
                missionUploadStatus_ = newStatus;
                switch (missionUploadStatus_) {
                    case MissionUploadStatus::IN_PROGRESS:
                        appendAuditLog("[MISSION] Upload in progress", "#D97706");
                        break;
                    case MissionUploadStatus::ACCEPTED:
                        appendAuditLog(QString("[MISSION_ACK] ACCEPTED (type=%1)").arg(ackType), "#4ADE80");
                        break;
                    case MissionUploadStatus::FAILED:
                        appendAuditLog(QString("[MISSION_ACK] FAILED (type=%1)").arg(ackType), "#F87171");
                        break;
                    case MissionUploadStatus::NOT_LOADED:
                        break;
                }
            }
        }
    }

    if (lblMissionSummaryStatus) {
        switch (missionUploadStatus_) {
            case MissionUploadStatus::IN_PROGRESS:
                lblMissionSummaryStatus->setText("Upload Status: IN-PROGRESS");
                lblMissionSummaryStatus->setStyleSheet("QLabel { color: #D97706; font-weight: bold; }");
                break;
            case MissionUploadStatus::ACCEPTED:
                lblMissionSummaryStatus->setText("Upload Status: ACCEPTED ✓");
                lblMissionSummaryStatus->setStyleSheet("QLabel { color: #10B981; font-weight: bold; }");
                break;
            case MissionUploadStatus::FAILED:
                lblMissionSummaryStatus->setText("Upload Status: FAILED ✗");
                lblMissionSummaryStatus->setStyleSheet("QLabel { color: #EF4444; font-weight: bold; }");
                break;
            case MissionUploadStatus::NOT_LOADED:
                lblMissionSummaryStatus->setText("Upload Status: NOT LOADED");
                lblMissionSummaryStatus->setStyleSheet("QLabel { color: #6B7280; }");
                break;
        }
    }

    // Gate UI actions based on flow + mission profile loaded
    if (btnPreflightHold) {
        btnPreflightHold->setEnabled(missionProfileLoaded_ && !preflightConfirmed_);
    }
    if (btnArm) {
        btnArm->setEnabled(missionProfileLoaded_ && preflightConfirmed_ && !armConfirmed_);
    }
    if (btnTakeoff) {
        btnTakeoff->setEnabled(missionProfileLoaded_ && preflightConfirmed_ && armConfirmed_ && !takeoffConfirmed_);
    }
    if (btnAbort) {
        btnAbort->setEnabled(missionProfileLoaded_);
    }
    if (btnEngageHold) {
        btnEngageHold->setEnabled(missionProfileLoaded_ && preflightConfirmed_ && armConfirmed_);
    }
    
    // RTB recovery buttons - enabled whenever mission is loaded (for emergency use)
    // Primary: mission loaded flag, Secondary: active mission states
    bool missionActive = missionProfileLoaded_ || 
                         (lastMissionState_ == "PREFLIGHT" ||
                          lastMissionState_ == "ARM_REQUESTED" ||
                          lastMissionState_ == "ARMED" ||
                          lastMissionState_ == "TRANSIT" || 
                          lastMissionState_ == "SEARCH" || 
                          lastMissionState_ == "ENGAGE" || 
                          lastMissionState_ == "ASSESS" || 
                          lastMissionState_ == "RTB");
    
    if (btnRtl) {
        btnRtl->setEnabled(missionActive);
    }
    if (btnLand) {
        btnLand->setEnabled(missionActive);
    }
}

void MainWindow::onArmClicked() {
    if (!missionProfileLoaded_) {
        appendAuditLog("[DENIED] ARM blocked until mission profile loaded", "#F87171");
        return;
    }
    if (!preflightConfirmed_) {
        appendAuditLog("[DENIED] ARM blocked until pre-flight", "#F87171");
        return;
    }
    setCommandPending(btnArm, "ARM");
    appendAuditLog("[REQUEST] ARM", "#D97706");
    // Wait for updateArmState(true) from backend before setting armConfirmed_
    
    // Send to backend
    if (backend_) {
        backend_->sendArmCommand();
    }
}

void MainWindow::onArmHoldPress() {
    if (!missionProfileLoaded_) {
        appendAuditLog("[DENIED] ARM blocked until mission profile loaded", "#F87171");
        return;
    }
    if (!preflightConfirmed_) {
        appendAuditLog("[DENIED] ARM blocked until pre-flight", "#F87171");
        return;
    }
    armHoldMs_ = 0;
    if (armHoldProgress) armHoldProgress->setValue(0);
    if (btnArm) btnArm->setText("HOLDING ARM...");
    armHoldTimer->start();
}

void MainWindow::onArmHoldRelease() {
    if (armHoldMs_ < armHoldRequiredMs_) {
        armHoldTimer->stop();
        if (btnArm) btnArm->setText("ARM");
        if (armHoldProgress) armHoldProgress->setValue(0);
        appendAuditLog("[DENIED] ARM cancelled (hold too short)", "#F87171");
    }
}

void MainWindow::onDisarmClicked() {
    setCommandPending(btnDisarm, "DISARM");
    appendAuditLog("[REQUEST] DISARM", "#D97706");
    armConfirmed_ = false;
    if (btnArm) btnArm->setText("ARM");
    if (armHoldProgress) armHoldProgress->setValue(0);
    
    // Send to backend
    if (backend_) {
        backend_->sendDisarmCommand();
    }
}

void MainWindow::onTakeoffClicked() {
    if (!missionProfileLoaded_) {
        appendAuditLog("[DENIED] TAKEOFF blocked until mission profile loaded", "#F87171");
        return;
    }
    if (!(preflightConfirmed_ && armConfirmed_)) {
        appendAuditLog("[DENIED] TAKEOFF blocked until pre-flight + ARM", "#F87171");
        return;
    }
    setCommandPending(btnTakeoff, "TAKEOFF");
    appendAuditLog("[REQUEST] TAKEOFF", "#D97706");
    takeoffConfirmed_ = true;
    
    // Send TAKEOFF command to backend
    if (backend_) {
        backend_->sendTakeoffCommand();
    }
}

void MainWindow::onTakeoffHoldPress() {
    if (!missionProfileLoaded_) {
        appendAuditLog("[DENIED] TAKEOFF blocked until mission profile loaded", "#F87171");
        return;
    }
    if (!(preflightConfirmed_ && armConfirmed_)) {
        appendAuditLog("[DENIED] TAKEOFF blocked until pre-flight + ARM", "#F87171");
        return;
    }
    takeoffHoldMs_ = 0;
    if (takeoffHoldProgress) takeoffHoldProgress->setValue(0);
    if (btnTakeoff) btnTakeoff->setText("HOLDING TAKEOFF...");
    takeoffHoldTimer->start();
}

void MainWindow::onTakeoffHoldRelease() {
    if (takeoffHoldMs_ < takeoffHoldRequiredMs_) {
        takeoffHoldTimer->stop();
        if (btnTakeoff) btnTakeoff->setText("TAKEOFF");
        if (takeoffHoldProgress) takeoffHoldProgress->setValue(0);
        appendAuditLog("[DENIED] TAKEOFF cancelled (hold too short)", "#F87171");
    }
}

void MainWindow::onLandClicked() {
    appendAuditLog("[SYSTEM] Emergency LAND requested", "#E2E8F0");
    
    // Send to backend
    if (backend_) {
        backend_->sendLandCommand();
    }
}

void MainWindow::onAbortClicked() {
    setCommandPending(btnAbort, "ABORT");
    appendAuditLog("[FAILSAFE] ABORT - Vehicle landing", "#F87171");
    armConfirmed_ = false;
    
    // Send to backend (sends LAND command)
    if (backend_) {
        backend_->sendAbortCommand();
    }
}

void MainWindow::onAbortHoldPress() {
    if (!missionProfileLoaded_) {
        appendAuditLog("[DENIED] ABORT blocked until mission profile loaded", "#F87171");
        return;
    }
    abortHoldMs_ = 0;
    if (abortHoldProgress) abortHoldProgress->setValue(0);
    if (btnAbort) btnAbort->setText("HOLDING ABORT...");
    abortHoldTimer->start();
}

void MainWindow::onAbortHoldRelease() {
    if (abortHoldMs_ < abortHoldRequiredMs_) {
        abortHoldTimer->stop();
        if (btnAbort) btnAbort->setText("ABORT");
        if (abortHoldProgress) abortHoldProgress->setValue(0);
        appendAuditLog("[DENIED] ABORT cancelled (hold too short)", "#F87171");
    }
}

void MainWindow::onRtlClicked() {
    appendAuditLog("[SYSTEM] RTL (Return to Launch) requested", "#E2E8F0");
    
    // Send to backend
    if (backend_) {
        backend_->sendRtlCommand();
    }
}

void MainWindow::onApproveClicked() {
    appendAuditLog("[AUTHORIZED] Operator approved", "#4ADE80");
}

void MainWindow::onDenyClicked() {
    appendAuditLog("[DENIED] Operator denied", "#F87171");
}

void MainWindow::onMissionProfileUpload() {
    QString file = QFileDialog::getOpenFileName(this, "Open Mission Profile", QString(), "JSON Files (*.json)");
    if (file.isEmpty()) {
        return;
    }

    mission::MissionProfile parsedProfile;
    std::string error;
    const bool ok = mission::MissionProfileParser::parseFromJson(
        file.toStdString(), parsedProfile, error);

    if (!ok) {
        missionProfileLoaded_ = false;
        lblMissionFile->setText("Invalid mission profile");
        if (lblMissionSummaryTarget) lblMissionSummaryTarget->setText("Target: —");
        if (lblMissionSummaryArea) lblMissionSummaryArea->setText("Search Area: —");
        if (lblMissionSummaryWaypoints) lblMissionSummaryWaypoints->setText("Waypoints: —");
        if (lblMissionSummaryAltitude) lblMissionSummaryAltitude->setText("Altitude Band: —");
        if (lblMissionSummaryPayload) lblMissionSummaryPayload->setText("Payload: —");
        if (lblMissionSummaryFailsafe) lblMissionSummaryFailsafe->setText("Fail-safe: —");
        missionUploadStatus_ = MissionUploadStatus::FAILED;
        missionUploadTicks_ = 0;
        if (lblMissionSummaryStatus) lblMissionSummaryStatus->setText("Upload Status: FAILED");
        if (lblMissionSummaryChecksum) lblMissionSummaryChecksum->setText("Checksum: —");
        appendAuditLog("[ERROR] Mission profile invalid: " + QString::fromStdString(error), "#F87171");
        return;
    }

    const uint32_t newChecksum = missionChecksum(parsedProfile);
    if (hasMissionChecksum_ && newChecksum == lastMissionChecksum_) {
        appendAuditLog("[SYSTEM] Mission unchanged (skipping reload)", "#D97706");
        missionUploadStatus_ = MissionUploadStatus::ACCEPTED;
        missionUploadTicks_ = 0;
        if (lblMissionSummaryStatus) lblMissionSummaryStatus->setText("Upload Status: ACCEPTED");
        if (lblMissionSummaryChecksum) {
            lblMissionSummaryChecksum->setText(QString("Checksum: 0x%1").arg(newChecksum, 8, 16, QLatin1Char('0')).toUpper());
        }
        return;
    }

    missionProfile_ = parsedProfile;
    missionProfileLoaded_ = true;
    preflightConfirmed_ = false;
    armConfirmed_ = false;
    takeoffConfirmed_ = false;
    lastWaypointSeq_ = std::numeric_limits<uint16_t>::max();
    hasMissionChecksum_ = true;
    lastMissionChecksum_ = newChecksum;
    missionUploadStatus_ = MissionUploadStatus::IN_PROGRESS;
    missionUploadTicks_ = 0;

    const QFileInfo info(file);
    lblMissionFile->setText(info.fileName());

    // Upload mission to backend
    if (backend_) {
        appendAuditLog("[SYSTEM] Uploading mission to vehicle...", "#3B82F6");
        backend_->loadMission(file.toStdString());
    } else {
        appendAuditLog("[ERROR] Backend not available", "#F87171");
        missionUploadStatus_ = MissionUploadStatus::FAILED;
        if (lblMissionSummaryStatus) lblMissionSummaryStatus->setText("Upload Status: FAILED");
    }

    auto mapFailsafe = [](mission::FailsafeBehavior behavior) -> QString {
        switch (behavior) {
            case mission::FailsafeBehavior::LAND: return "LAND";
            case mission::FailsafeBehavior::RTL: return "RTL";
            case mission::FailsafeBehavior::HOLD: return "HOLD";
            case mission::FailsafeBehavior::CONTINUE: return "CONTINUE";
        }
        return "RTL";
    };

    auto mapGpsFailsafe = [](mission::FailsafeBehavior behavior) -> QString {
        switch (behavior) {
            case mission::FailsafeBehavior::LAND: return "LAND";
            case mission::FailsafeBehavior::RTL: return "RTL (Dead Reckoning)";
            case mission::FailsafeBehavior::HOLD: return "HOLD";
            case mission::FailsafeBehavior::CONTINUE: return "LOITER";
        }
        return "RTL (Dead Reckoning)";
    };

    if (cmbCommsLoss) {
        const QString value = mapFailsafe(missionProfile_.failsafe_rules.comms_loss);
        const int idx = cmbCommsLoss->findText(value);
        if (idx >= 0) cmbCommsLoss->setCurrentIndex(idx);
    }
    if (cmbLowBattery) {
        const QString value = mapFailsafe(missionProfile_.failsafe_rules.low_battery);
        const int idx = cmbLowBattery->findText(value);
        if (idx >= 0) cmbLowBattery->setCurrentIndex(idx);
    }
    if (cmbGpsJamming) {
        const QString value = mapGpsFailsafe(missionProfile_.failsafe_rules.gps_jamming);
        const int idx = cmbGpsJamming->findText(value);
        if (idx >= 0) cmbGpsJamming->setCurrentIndex(idx);
    }

    auto payloadTypeLabel = [](mission::PayloadType type) -> QString {
        switch (type) {
            case mission::PayloadType::KINETIC: return "KINETIC";
            case mission::PayloadType::EXPLOSIVE: return "EXPLOSIVE";
            case mission::PayloadType::NONE: return "NONE";
        }
        return "NONE";
    };

    if (lblMissionSummaryTarget) {
        lblMissionSummaryTarget->setText(
            missionProfile_.target_id.has_value()
                ? QString("Target: %1").arg(QString::fromStdString(*missionProfile_.target_id))
                : "Target: —");
    }
    if (lblMissionSummaryArea) {
        lblMissionSummaryArea->setText(
            QString("Search Area: %1 vertices").arg(static_cast<int>(missionProfile_.search_area.vertices.size())));
    }
    if (lblMissionSummaryWaypoints) {
        lblMissionSummaryWaypoints->setText(
            QString("Waypoints: %1").arg(static_cast<int>(missionProfile_.waypoints.size())));
    }
    if (lblMissionSummaryAltitude) {
        lblMissionSummaryAltitude->setText(
            QString("Altitude Band: %1m - %2m")
                .arg(missionProfile_.search_area.min_altitude_m, 0, 'f', 1)
                .arg(missionProfile_.search_area.max_altitude_m, 0, 'f', 1));
    }
    if (lblMissionSummaryPayload) {
        lblMissionSummaryPayload->setText(
            QString("Payload: %1 | Arm on Engage: %2")
                .arg(payloadTypeLabel(missionProfile_.payload_policy.type))
                .arg(missionProfile_.payload_policy.arm_on_engagement ? "YES" : "NO"));
    }
    if (lblMissionSummaryFailsafe) {
        lblMissionSummaryFailsafe->setText(
            QString("Fail-safe: Comms %1 | Battery %2 | GPS %3")
                .arg(mapFailsafe(missionProfile_.failsafe_rules.comms_loss))
                .arg(mapFailsafe(missionProfile_.failsafe_rules.low_battery))
                .arg(mapGpsFailsafe(missionProfile_.failsafe_rules.gps_jamming)));
    }
    if (lblMissionSummaryStatus) {
        lblMissionSummaryStatus->setText("Upload Status: IN-PROGRESS");
    }
    if (lblMissionSummaryChecksum) {
        lblMissionSummaryChecksum->setText(QString("Checksum: 0x%1").arg(newChecksum, 8, 16, QLatin1Char('0')).toUpper());
    }

    appendAuditLog("[SYSTEM] Mission profile loaded", "#E2E8F0");
    appendAuditLog(
        QString("[MISSION] Search area vertices: %1 | Waypoints: %2")
            .arg(static_cast<int>(missionProfile_.search_area.vertices.size()))
            .arg(static_cast<int>(missionProfile_.waypoints.size())),
        "#4ADE80");
    appendAuditLog(
        QString("[MISSION] Altitude band: %1m - %2m")
            .arg(missionProfile_.search_area.min_altitude_m, 0, 'f', 1)
            .arg(missionProfile_.search_area.max_altitude_m, 0, 'f', 1),
        "#4ADE80");
    if (missionProfile_.target_id.has_value()) {
        appendAuditLog(QString("[MISSION] Target ID: %1").arg(QString::fromStdString(*missionProfile_.target_id)), "#4ADE80");
    }
    appendAuditLog(
        QString("[MISSION] Schema v%1 | Payload delay: %2s")
            .arg(missionProfile_.schema_version)
            .arg(missionProfile_.payload_policy.arming_delay_seconds, 0, 'f', 1),
        "#4ADE80");
}

void MainWindow::onPreflightPress() {
    if (!missionProfileLoaded_) {
        appendAuditLog("[DENIED] Pre-flight blocked until mission profile loaded", "#F87171");
        return;
    }
    preflightHoldMs_ = 0;
    preflightHoldProgress->setValue(0);
    btnPreflightHold->setText("HOLDING...");
    preflightHoldTimer->start();
}

void MainWindow::onPreflightRelease() {
    if (preflightHoldMs_ < preflightHoldRequiredMs_) {
        preflightHoldTimer->stop();
        btnPreflightHold->setText("HOLD TO CONFIRM PRE-FLIGHT");
        preflightHoldProgress->setValue(0);
        appendAuditLog("[DENIED] Pre-flight cancelled (hold too short)", "#F87171");
        preflightConfirmed_ = false;
    } else {
        preflightConfirmed_ = true;
        appendAuditLog("[FLOW] Pre-flight complete", "#4ADE80");
    }
}

void MainWindow::onEngagePress() {
    if (!(preflightConfirmed_ && armConfirmed_)) {
        appendAuditLog("[DENIED] Engage blocked until pre-flight + ARM", "#F87171");
        return;
    }
    engageHoldMs_ = 0;
    engageHoldProgress->setValue(0);
    btnEngageHold->setText("HOLDING...");
    engageHoldTimer->start();
}

void MainWindow::onEngageRelease() {
    if (engageHoldMs_ < engageHoldRequiredMs_) {
        engageHoldTimer->stop();
        btnEngageHold->setText("PRESS & HOLD TO ENGAGE");
        engageHoldProgress->setValue(0);
        appendAuditLog("[DENIED] Engage cancelled (hold too short)", "#F87171");
    }
}

void MainWindow::onBdaYes() {
    appendAuditLog("[BDA] Target neutralized = YES", "#4ADE80");
    lblBdaStatus->setText("BDA: TARGET NEUTRALIZED");
}

void MainWindow::onBdaNo() {
    appendAuditLog("[BDA] Target neutralized = NO", "#F87171");
    lblBdaStatus->setText("BDA: TARGET ACTIVE");
}

void MainWindow::onBdaUnknown() {
    appendAuditLog("[BDA] Target neutralized = UNKNOWN", "#D97706");
    lblBdaStatus->setText("BDA: UNKNOWN");
}

void MainWindow::setCommandPending(QPushButton* button, const QString& baseText) {
    if (!button) return;
    button->setText(baseText + " (PENDING)");
    button->setEnabled(false);
    QTimer::singleShot(700, this, [this, button, baseText]() {
        setCommandAcked(button, baseText);
    });
}

void MainWindow::setCommandAcked(QPushButton* button, const QString& baseText) {
    if (!button) return;
    button->setText(baseText + " ✓");
    button->setEnabled(true);
    QString ackColor = "#4ADE80";
    if (baseText == "TAKEOFF") {
        ackColor = "#F59E0B";
    } else if (baseText == "ABORT") {
        ackColor = "#EF4444";
    }
    appendAuditLog("[ACK] " + baseText + " accepted", ackColor);

    QTimer::singleShot(1200, this, [this, button, baseText]() {
        resetCommandButton(button, baseText);
    });
}

void MainWindow::resetCommandButton(QPushButton* button, const QString& baseText) {
    if (!button) return;
    button->setText(baseText);
}
// ======================================================================
// BACKEND INTEGRATION SLOTS
// ======================================================================

void MainWindow::onMissionStateChanged(const QString& state) {
    // Only log if state actually changed
    if (lastMissionState_ == state) {
        return;  // Skip duplicate state logs
    }
    lastMissionState_ = state;
    
    if (lblMissionState) {
        lblMissionState->setText("STATE: " + state);
        
        // Color code by state
        QString color = "#FFFFFF";
        if (state == "INIT") color = "#6B7280";
        else if (state == "PREFLIGHT") color = "#F59E0B";
        else if (state == "ARM_REQUESTED") color = "#FB923C";
        else if (state == "ARMED") color = "#10B981";
        else if (state == "TRANSIT") color = "#3B82F6";
        else if (state == "SEARCH") color = "#8B5CF6";
        else if (state == "ENGAGE") color = "#DC2626";
        else if (state == "ASSESS") color = "#EA580C";
        else if (state == "RTB") color = "#14B8A6";
        else if (state == "COMPLETE") color = "#059669";
        else if (state == "ABORTED") color = "#7F1D1D";
        
        lblMissionState->setStyleSheet("QLabel { color: " + color + "; font-weight: bold; font-size: 14px; }");
    }
    
    if (lblMissionStatus) {
        lblMissionStatus->setText("STATUS: " + state);
        QString statusColor = "#111827";
        if (state == "INIT") statusColor = "#6B7280";
        else if (state == "PREFLIGHT") statusColor = "#F59E0B";
        else if (state == "ARM_REQUESTED") statusColor = "#FB923C";
        else if (state == "ARMED") statusColor = "#10B981";
        else if (state == "TRANSIT") statusColor = "#3B82F6";
        else if (state == "SEARCH") statusColor = "#8B5CF6";
        else if (state == "ENGAGE") statusColor = "#DC2626";
        else if (state == "ASSESS") statusColor = "#EA580C";
        else if (state == "RTB") statusColor = "#14B8A6";
        else if (state == "COMPLETE") statusColor = "#059669";
        else if (state == "ABORTED") statusColor = "#7F1D1D";
        lblMissionStatus->setStyleSheet("QLabel { color: " + statusColor + "; font-weight: bold; }");
    }

    appendAuditLog("[STATE] Mission transitioned to: " + state, "#4ADE80");
}

void MainWindow::updateTelemetryDisplay(double lat, double lon, double alt) {
    if (lblLatValue) lblLatValue->setText(QString::number(lat, 'f', 6));
    if (lblLonValue) lblLonValue->setText(QString::number(lon, 'f', 6));
    if (lblAltValue) lblAltValue->setText(QString::number(alt, 'f', 1) + " m");
}

void MainWindow::onConnectionStatusChanged(const QString& status) {
    if (lblConnection) {
        lblConnection->setText(status);
        if (status == "CONNECTED") {
            lblConnection->setStyleSheet("QLabel { color: #10B981; font-weight: bold; }");
        } else {
            lblConnection->setStyleSheet("QLabel { color: #EF4444; font-weight: bold; }");
        }
    }
}

void MainWindow::updateFlightMode(const QString& mode) {
    if (lblFlightMode) {
        lblFlightMode->setText(mode);
        lblFlightMode->setStyleSheet("QLabel { color: #3B82F6; font-weight: bold; }");
    }
}

void MainWindow::onMissionUploadSuccess() {
    missionProfileLoaded_ = true;
    missionUploadStatus_ = MissionUploadStatus::ACCEPTED;
    lastErrorMessage_.clear();
    if (lblMissionSummaryStatus) {
        lblMissionSummaryStatus->setText("Upload Status: ACCEPTED ✓");
    }
    appendAuditLog("[SYSTEM] Mission upload complete - preflight enabled", "#4ADE80");
    appendAuditLog("[INFO] For SITL testing: Use 'commander arm' in PX4 terminal after GPS lock", "#6B7280");
}

void MainWindow::updateArmState(bool armed) {
    if (lblArmed) {
        lblArmed->setText(armed ? "ARMED" : "DISARMED");
        lblArmed->setStyleSheet(armed ? "QLabel { color: #DC2626; font-weight: bold; }" 
                                      : "QLabel { color: #6B7280; font-weight: bold; }");
    }
    
    // Update internal state based on vehicle feedback - only log on state change
    if (armed && !armConfirmed_) {
        // Transition from disarmed to armed
        armConfirmed_ = true;
        if (btnArm) btnArm->setText("ARM ✓");
        appendAuditLog("[ARMED] Vehicle armed successfully", "#10B981");
    } else if (!armed && armConfirmed_) {
        // Transition from armed to disarmed
        armConfirmed_ = false;
        takeoffConfirmed_ = false;
        if (btnArm) {
            btnArm->setText("ARM");
            btnArm->setEnabled(true);
        }
        if (btnTakeoff) btnTakeoff->setText("TAKEOFF");
        if (armHoldProgress) armHoldProgress->setValue(0);
        appendAuditLog("[DISARMED] Vehicle disarmed", "#F59E0B");
    }
    
    if (btnDisarm && armed) {
        btnDisarm->setEnabled(true);
        btnDisarm->setText("DISARM");
    }
}

void MainWindow::updateBatteryDisplay(int percent) {
    if (lblBatteryPercent) {
        lblBatteryPercent->setText(QString::number(percent) + "%");
        
        // Color code: green > 50%, yellow 20-50%, red < 20%
        QString color = percent > 50 ? "#10B981" : (percent > 20 ? "#F59E0B" : "#DC2626");
        lblBatteryPercent->setStyleSheet("QLabel { color: " + color + "; font-weight: bold; }");
    }
    
    if (batteryBar) {
        batteryBar->setValue(percent);
    }
}

void MainWindow::updateMissionUploadProgress(int sent, int total) {
    if (total > 0 && (sent != lastUploadSent_ || total != lastUploadTotal_)) {
        int progress = (sent * 100) / total;
        appendAuditLog("[UPLOAD] Mission: " + QString::number(sent) + "/" + QString::number(total)
                      + " waypoints (" + QString::number(progress) + "%)", "#3B82F6");
        lastUploadSent_ = sent;
        lastUploadTotal_ = total;
    }
}

void MainWindow::updateCurrentWaypoint(uint16_t seq) {
    if (lblSearchPath) {
        lblSearchPath->setText("Current Waypoint: " + QString::number(seq));
    }
    if (seq != lastWaypointSeq_) {
        appendAuditLog("[SEARCH] PX4 executing waypoint sequence: " + QString::number(seq), "#8B5CF6");
        lastWaypointSeq_ = seq;
    }
}

void MainWindow::onStatusUpdated(const QString& status) {
    const QString trimmed = status.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (trimmed == lastStatusText_) {
        return;
    }
    lastStatusText_ = trimmed;

    appendAuditLog("[PX4] " + trimmed, "#94A3B8");

    const QString lower = trimmed.toLower();
    const bool isCritical =
        lower.contains("preflight") ||
        lower.contains("mag") ||
        lower.contains("ekf") ||
        lower.contains("gps") ||
        lower.contains("accel") ||
        lower.contains("gyro") ||
        lower.contains("health") ||
        lower.contains("fail") ||
        lower.contains("critical") ||
        lower.contains("denied");

    if (isCritical && btnArm) {
        if (armButtonBaseStyle_.isEmpty()) {
            armButtonBaseStyle_ = btnArm->styleSheet();
        }

        btnArm->setStyleSheet(armButtonBaseStyle_ +
            " QPushButton { border: 2px solid #EF4444; } ");

        const QPoint popupPos = btnArm->mapToGlobal(
            QPoint(btnArm->width() / 2, 0));
        QToolTip::showText(popupPos, trimmed, btnArm, QRect(), 3000);

        QTimer::singleShot(3000, this, [this]() {
            if (btnArm) {
                btnArm->setStyleSheet(armButtonBaseStyle_);
            }
        });
    }
}

void MainWindow::displayError(const QString& error) {
    if (lastErrorMessage_ != error) {
        appendAuditLog("[ERROR] " + error, "#DC2626");
        lastErrorMessage_ = error;
    }
    // Don't overwrite mission status - errors will show in audit log only
}

// === Recovery & RTB Slots (New) ===

void MainWindow::onCommsLoss() {
    appendAuditLog("[ALERT] COMMS LOSS DETECTED - Initiating failsafe", "#EF4444");
    if (lblMissionStatus) {
        lblMissionStatus->setText("COMMS LOSS - FAILSAFE");
        lblMissionStatus->setStyleSheet("QLabel { color: #EF4444; font-weight: bold; animation: blink 1s infinite; }");
    }
    if (lblConnection) {
        lblConnection->setText("LINK: LOST");
        lblConnection->setStyleSheet("QLabel { color: #EF4444; font-weight: bold; }");
    }
}

void MainWindow::onBDAStarted() {
    appendAuditLog("[BDA] Battle Damage Assessment started", "#FBBF24");
    if (lblMissionStatus) {
        lblMissionStatus->setText("STATUS: BDA IN PROGRESS");
        lblMissionStatus->setStyleSheet("QLabel { color: #FBBF24; font-weight: bold; }");
    }
}

void MainWindow::onBDAResult(const QString& health_status) {
    QString color;
    if (health_status == "MISSION_WORTHY") {
        color = "#10B981";
        appendAuditLog("[BDA] Result: MISSION WORTHY ✓ - Vehicle healthy, can continue if needed", color);
    } else if (health_status == "DEGRADED") {
        color = "#FBBF24";
        appendAuditLog("[BDA] Result: DEGRADED ⚠ - Vehicle damaged but operational", color);
    } else {  // CRITICAL
        color = "#EF4444";
        appendAuditLog("[BDA] Result: CRITICAL ✗ - Vehicle compromised, initiating RTB", color);
    }
    
    if (lblMissionStatus) {
        lblMissionStatus->setText("BDA: " + health_status);
        lblMissionStatus->setStyleSheet("QLabel { color: " + color + "; font-weight: bold; }");
    }
}

void MainWindow::onRTBStarted(const QString& reason) {
    appendAuditLog("[RTB] Return to Base initiated - " + reason, "#3B82F6");
    if (lblMissionStatus) {
        lblMissionStatus->setText("STATUS: RETURN TO BASE");
        lblMissionStatus->setStyleSheet("QLabel { color: #3B82F6; font-weight: bold; }");
    }
}

void MainWindow::onLandingDetected() {
    appendAuditLog("[LANDING] Vehicle landing detected", "#10B981");
    if (lblMissionStatus) {
        lblMissionStatus->setText("STATUS: LANDING");
        lblMissionStatus->setStyleSheet("QLabel { color: #10B981; font-weight: bold; }");
    }
}

void MainWindow::onMissionCompleted() {
    appendAuditLog("[MISSION] Mission lifecycle complete - Vehicle safe", "#10B981");
    if (lblMissionStatus) {
        lblMissionStatus->setText("STATUS: MISSION COMPLETE");
        lblMissionStatus->setStyleSheet("QLabel { color: #10B981; font-weight: bold; }");
    }
    // Disable all command buttons
    if (btnArm) btnArm->setEnabled(false);
    if (btnTakeoff) btnTakeoff->setEnabled(false);
    if (btnEngageHold) btnEngageHold->setEnabled(false);
}

void MainWindow::onHealthStatusUpdated(bool ekf_ok, bool battery_ok, bool heartbeat_ok) {
    if (chkMotors) {
        chkMotors->setChecked(ekf_ok);
        chkMotors->setStyleSheet(ekf_ok ? 
            "QCheckBox { color: #10B981; font-weight: bold; }" : 
            "QCheckBox { color: #EF4444; }");
    }
    if (chkBattery) {
        chkBattery->setChecked(battery_ok);
        chkBattery->setStyleSheet(battery_ok ? 
            "QCheckBox { color: #10B981; font-weight: bold; }" : 
            "QCheckBox { color: #EF4444; }");
    }
    if (chkMavlink) {
        chkMavlink->setChecked(heartbeat_ok);
        chkMavlink->setStyleSheet(heartbeat_ok ? 
            "QCheckBox { color: #10B981; font-weight: bold; }" : 
            "QCheckBox { color: #EF4444; }");
    }
}