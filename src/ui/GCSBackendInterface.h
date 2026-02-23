#pragma once

#include <QObject>
#include <QString>
#include <chrono>
#include "mission/Mission.h"
#include "telemetry/TelemetryData.h"
#include "command/CommandManager.h"

/**
 * @brief Bridge between backend (GCS logic) and UI (Qt)
 * 
 * Emits signals when:
 * - Mission state changes
 * - Telemetry updates
 * - Commands execute
 * - Errors occur
 */
class GCSBackendInterface : public QObject {
    Q_OBJECT

public:
    explicit GCSBackendInterface(QObject *parent = nullptr);
    
    // Update from backend
    void onMissionStateChanged(mission::MissionState old_state, mission::MissionState new_state);
    void onTelemetryUpdated(const TelemetryData& telemetry);
    void onMissionUploadProgress(int waypoints_sent, int total_waypoints);
    void onMissionUploadComplete();
    void onCommandExecuted(const QString& command_name, bool success);
    void onError(const QString& error_message);
    
    // Recovery callbacks (New)
    void onCommsLoss();
    void onBDAStarted();
    void onBDAComplete(const QString& health_status);
    void onRTBInitiated(const QString& reason);
    void onRecoveryPlanUpdated(const QString& plan);
    void onLandingDetected();
    void onMissionCompleted();
    void sendPayloadArmingCountdown(int seconds) { emit payloadArmingCountdown(seconds); }
    void sendPayloadArmingRequested() { emit payloadArmingRequested(); }
signals:
    // Connection
    void connectionStatusChanged(const QString& status);
    
    // Mission state
    void missionStateChanged(const QString& state_name);
    
    // Telemetry
    void positionUpdated(double lat, double lon, float alt);
    void armStateChanged(bool armed);
    void modeChanged(const QString& mode);
    void batteryUpdated(float voltage, float current, int remaining);
    void ekfStatusChanged(bool ready);
    
    // Mission execution
    void missionUploadStarted();
    void missionUploadProgress(int sent, int total);
    void missionUploadComplete();
    void missionUploadSuccess();  // Signal when mission upload completes successfully
    void missionUploadFailed(const QString& reason);
    void missionCurrentWaypoint(int seq);
    
    // Commands
    void commandQueued(const QString& cmd);
    void commandAcknowledged(const QString& cmd);
    void commandFailed(const QString& cmd, const QString& reason);
    
    // Health checks (BIT)
    void healthStatusUpdated(bool ekf_ok, bool battery_ok, bool heartbeat_ok);
    
    // Recovery & RTB (New)
    void commsLossDetected();
    void bdaAssessmentStarted();
    void bdaResult(const QString& health_status);  // MISSION_WORTHY, DEGRADED, CRITICAL
    void rtbInitiated(const QString& reason);
    void recoveryPlanUpdated(const QString& plan);
    void landingDetected();
    void missionCompleted();
    
    // General
    void statusUpdated(const QString& status);
    void errorOccurred(const QString& error);
    void payloadArmingCountdown(int seconds_remaining);
    void payloadArmingRequested();
private:
    TelemetryData last_telemetry_;
};
