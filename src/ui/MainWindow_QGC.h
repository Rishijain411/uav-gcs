#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QFrame>
#include <QProgressBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QGroupBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QSlider>
#include <QScrollArea>
#include <QTabWidget>
#include <QSplitter>
#include <limits>

#include "mission/MissionProfile.h"
#include "ui/IntegratedBackend.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    
    // Set the backend interface
    void setBackend(IntegratedBackend* backend) { backend_ = backend; }
    ~MainWindow();

public slots:
    void updateDisplay();
    void onArmClicked();
    void onDisarmClicked();
    void onTakeoffClicked();
    void onLandClicked();
    void onAbortClicked();
    void onArmHoldPress();
    void onArmHoldRelease();
    void onAbortHoldPress();
    void onAbortHoldRelease();
    void onTakeoffHoldPress();
    void onTakeoffHoldRelease();
    void onApproveClicked();
    void onDenyClicked();
    void onRtlClicked();
    void onMissionProfileUpload();
    void onPreflightPress();
    void onPreflightRelease();
    void onEngagePress();
    void onEngageRelease();
    void onBdaYes();
    void onBdaNo();
    void onBdaUnknown();
    void resetCommandButton(QPushButton* button, const QString& baseText);
    void onSpeedUpdated(float speed, float climb, int heading);
    void onTargetUpdateReceived(double range, double closing_speed, double confidence);
    // Backend integration slots
    void onConnectionStatusChanged(const QString& status);
    void onMissionStateChanged(const QString& state);
    void updateTelemetryDisplay(double lat, double lon, double alt);
    void updateArmState(bool armed);
    void updateFlightMode(const QString& mode);
    void updateBatteryDisplay(int percent);
    void updateMissionUploadProgress(int sent, int total);
    void updateCurrentWaypoint(uint16_t seq);
    void displayError(const QString& error);
    void onMissionUploadSuccess();
    void onStatusUpdated(const QString& status);
    
    // Recovery & RTB slots (New)
    void onCommsLoss();
    void onBDAStarted();
    void onBDAResult(const QString& health_status);
    void onRTBStarted(const QString& reason);
    void onLandingDetected();
    void onMissionCompleted();
    void onHealthStatusUpdated(bool ekf_ok, bool battery_ok, bool heartbeat_ok);
    void onPayloadArmingCountdown(int seconds);
    void onPayloadArmingRequested();
private:
    enum class MissionUploadStatus {
        NOT_LOADED,
        IN_PROGRESS,
        ACCEPTED,
        FAILED
    };

    void setupUI();
    void createTopToolbar();
    void createCenterLayout();
    void createLeftSidebar(QWidget* parent);
    void createCenterPanel(QWidget* parent);
    void createRightPanel(QWidget* parent);
    void createBottomPanel(QWidget* parent);
    void createStatusBar();
    QFrame* createAssetCard(const QString& name, const QString& phase, int batteryPercent, const QString& link, const QString& urgency = "NORMAL");
    void appendAuditLog(const QString& text, const QString& color);
    void setCommandPending(QPushButton* button, const QString& baseText);
    void setCommandAcked(QPushButton* button, const QString& baseText);
    //void refreshCommandAuthority();
    // === TOP TOOLBAR (Initialized to nullptr to prevent SIGSEGV) ===
    QLabel* lblConnection = nullptr;
    QLabel* lblEncryption = nullptr;
    QLabel* lblLatency = nullptr;
    QLabel* lblTelemetryRate = nullptr;
    QLabel* lblActiveCount = nullptr;
    QLabel* lblArmed = nullptr;
    QLabel* lblFlightMode = nullptr;
    QLabel* lblMissionStatus = nullptr;

    // === LEFT SIDEBAR ===
    QLabel* lblPhaseInit = nullptr;
    QLabel* lblPhaseArm = nullptr;
    QLabel* lblPhaseSearch = nullptr;
    QLabel* lblPhaseEngage = nullptr;
    QLabel* lblPhaseRtl = nullptr;
    QLabel* lblMissionFile = nullptr;
    QLabel* lblMissionSummaryTarget = nullptr;
    QLabel* lblMissionSummaryArea = nullptr;
    QLabel* lblMissionSummaryWaypoints = nullptr;
    QLabel* lblMissionSummaryAltitude = nullptr;
    QLabel* lblMissionSummaryPayload = nullptr;
    QLabel* lblMissionSummaryFailsafe = nullptr;
    QLabel* lblMissionSummaryStatus = nullptr;
    QLabel* lblMissionSummaryChecksum = nullptr;
    QPushButton* btnPreflightHold = nullptr;
    QProgressBar* preflightHoldProgress = nullptr;
    QComboBox* cmbCommsLoss = nullptr;
    QComboBox* cmbLowBattery = nullptr;
    QComboBox* cmbGpsJamming = nullptr;
    QCheckBox* chkMotors = nullptr;
    QCheckBox* chkBattery = nullptr;
    QCheckBox* chkMavlink = nullptr;
    QCheckBox* chkPayload = nullptr;
    QComboBox* cmbSearchMode = nullptr;
    QListWidget* listOitlQueue = nullptr;
    QSpinBox* spnReengageCount = nullptr;
    QLabel* lblBdaStatus = nullptr;
    QPushButton* btnRtl = nullptr;
    QPushButton* btnLand = nullptr;
    QCheckBox* chkBdaMotors = nullptr;
    QCheckBox* chkBdaLink = nullptr;
    QCheckBox* chkBdaPayload = nullptr;
    QPushButton* btnBdaYes = nullptr;
    QPushButton* btnBdaNo = nullptr;
    QPushButton* btnBdaUnknown = nullptr;

    // === CENTER ===
    QLabel* lblMissionState = nullptr;
    QLabel* lblEngageBanner = nullptr;
    QLabel* lblCombatBubble = nullptr;
    QLabel* lblTargetLock = nullptr;
    QLabel* lblMapPlaceholder = nullptr;
    QLabel* lblNfzStatus = nullptr;
    QLabel* lblSearchPath = nullptr;
    QLabel* lblLockOverlay = nullptr;
    QLabel* lblMapLegend = nullptr;
    QFrame* mapFrame_ = nullptr;

    // === RIGHT PANEL ===
    QVBoxLayout* assetsLayout = nullptr;

    // === BOTTOM ===
    QLabel* lblVideoPlaceholder = nullptr;
    QLabel* lblAltValue = nullptr;
    QLabel* lblSpeedValue = nullptr;
    QLabel* lblBatteryPercent = nullptr;
    QLabel* lblGpsValue = nullptr;
    QLabel* lblLatValue = nullptr;
    QLabel* lblLonValue = nullptr;
    QProgressBar* batteryBar = nullptr;
    QFrame* videoFrame_ = nullptr;
    QFrame* telemetryFrame_ = nullptr;
    QFrame* auditFrame_ = nullptr;

    // === COMMAND AUTH + AUDIT ===
    QPushButton* btnArm = nullptr;
    QPushButton* btnDisarm = nullptr;
    QPushButton* btnTakeoff = nullptr;
    QPushButton* btnAbort = nullptr;
    QPushButton* btnEngageHold = nullptr;
    QProgressBar* engageHoldProgress = nullptr;
    QProgressBar* armHoldProgress = nullptr;
    QProgressBar* abortHoldProgress = nullptr;
    QProgressBar* takeoffHoldProgress = nullptr;
    QTextEdit* txtAuditLog = nullptr;

    QString armButtonBaseStyle_;
    QString lastStatusText_;

    // UI gate state
    bool missionProfileLoaded_ = false;
    bool preflightConfirmed_ = false;
    bool armConfirmed_ = false;
    bool takeoffConfirmed_ = false;

    // Mission profile data
    mission::MissionProfile missionProfile_{};
    bool hasMissionChecksum_ = false;
    uint32_t lastMissionChecksum_ = 0;
    MissionUploadStatus missionUploadStatus_ = MissionUploadStatus::NOT_LOADED;
    MissionUploadStatus lastLoggedMissionUploadStatus_ = MissionUploadStatus::NOT_LOADED;
    int missionUploadTicks_ = 0;
    QString lastMissionState_; 
    uint16_t lastWaypointSeq_ = std::numeric_limits<uint16_t>::max();
    QString lastErrorMessage_;
    int lastUploadSent_ = -1;
    int lastUploadTotal_ = -1;

    // Engage hold state
    QTimer* engageHoldTimer = nullptr;
    int engageHoldMs_ = 0;
    const int engageHoldRequiredMs_ = 900;

    QTimer* preflightHoldTimer = nullptr;
    int preflightHoldMs_ = 0;
    const int preflightHoldRequiredMs_ = 700;

    QTimer* armHoldTimer = nullptr;
    int armHoldMs_ = 0;
    const int armHoldRequiredMs_ = 3000;

    QTimer* takeoffHoldTimer = nullptr;
    int takeoffHoldMs_ = 0;
    const int takeoffHoldRequiredMs_ = 3000;

    QTimer* abortHoldTimer = nullptr;
    int abortHoldMs_ = 0;
    const int abortHoldRequiredMs_ = 800;

    // Update timer
    QTimer* updateTimer = nullptr;
    
    // Backend integration
    IntegratedBackend* backend_ = nullptr;
};
