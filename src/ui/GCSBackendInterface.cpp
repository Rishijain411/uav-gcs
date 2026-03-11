#include "GCSBackendInterface.h"
#include <iostream>

GCSBackendInterface::GCSBackendInterface(QObject *parent)
    : QObject(parent) {
}

void GCSBackendInterface::onMissionStateChanged(
    mission::MissionState old_state, 
    mission::MissionState new_state) {
    
    const char* state_names[] = {
        "INIT", "PREFLIGHT", "ARM_REQUESTED", "ARMED", 
        "TRANSIT", "SEARCH", "ENGAGE", "ASSESS", 
        "RTB", "COMPLETE", "ABORTED"
    };
    
    int idx = static_cast<int>(new_state);
    if (idx >= 0 && idx < 11) {
        emit missionStateChanged(QString::fromUtf8(state_names[idx]));
    }
}

void GCSBackendInterface::onTelemetryUpdated(const TelemetryData& telemetry) {
    // 1. Position & Altitude (Coordinates)
    // Only emit if there is a meaningful change to preserve UI performance
    if (telemetry.latitude_deg != last_telemetry_.latitude_deg ||
        telemetry.longitude_deg != last_telemetry_.longitude_deg ||
        telemetry.relative_alt_m != last_telemetry_.relative_alt_m) {
        emit positionUpdated(telemetry.latitude_deg, telemetry.longitude_deg, 
                            telemetry.relative_alt_m);
    }
    
    // 2. Real-Time HUD Metrics (Groundspeed, Climb Rate, Heading)
    // Satisfies PRD requirement for 10Hz telemetry update rate
    if (telemetry.groundspeed != last_telemetry_.groundspeed ||
        telemetry.climb_rate != last_telemetry_.climb_rate ||
        telemetry.heading != last_telemetry_.heading) {
        emit speedUpdated(telemetry.groundspeed, telemetry.climb_rate, telemetry.heading);
    }

    // 3. Arm State Tracking
    if (telemetry.arm_state != last_telemetry_.arm_state) {
        emit armStateChanged(telemetry.arm_state == ArmState::ARMED);
    }
    
    // Battery (simplified: just use battery_ok as presence indicator)
    if (telemetry.battery_ok != last_telemetry_.battery_ok) {
        int percent = telemetry.battery_ok ? 75 : 10;  // Mock battery percentage
        emit batteryUpdated(0, 0, percent);  // Voltage/current placeholders
    }
    
    // EKF status
    if (telemetry.ekf_ok != last_telemetry_.ekf_ok) {
        emit ekfStatusChanged(telemetry.ekf_ok);
    }
    
    // 6. Mission Execution Tracking (PX4 Mission sequence)
    if (telemetry.mission_current_received && 
        telemetry.mission_current_seq != last_telemetry_.mission_current_seq) {
        emit missionCurrentWaypoint(telemetry.mission_current_seq);
    }

    // 7. Target Tracking (Phase 3: Engagement Telemetry)
    // Critical for visualizing the "Kill Chain" and intercept course
    if (telemetry.target_detected) {
        emit targetUpdateReceived(telemetry.target_range_m, 
                                 telemetry.target_closing_speed, 
                                 telemetry.target_confidence);
    }
    
    // 8. Health Status Bridge (BIT Checkboxes)
    // Pushes real-time status to the Pre-Flight BIT checkboxes in the sidebar
    emit healthStatusUpdated(telemetry.ekf_ok, telemetry.battery_ok, telemetry.heartbeat_received);

    // Update local cache for the next delta check
    last_telemetry_ = telemetry;
}

void GCSBackendInterface::onMissionUploadProgress(int waypoints_sent, int total_waypoints) {
    emit missionUploadProgress(waypoints_sent, total_waypoints);
}

void GCSBackendInterface::onMissionUploadComplete() {
    emit missionUploadComplete();
}

void GCSBackendInterface::onCommandExecuted(const QString& command_name, bool success) {
    if (success) {
        emit commandAcknowledged(command_name);
    } else {
        emit commandFailed(command_name, "Command not acknowledged");
    }
}

void GCSBackendInterface::onError(const QString& error_message) {
    emit errorOccurred(error_message);
}

// Recovery callbacks (New)
void GCSBackendInterface::onCommsLoss() {
    std::cout << "[UI] Comms loss detected!\n";
    emit commsLossDetected();
    emit statusUpdated("COMMS LOSS - Initiating recovery");
}

void GCSBackendInterface::onBDAStarted() {
    std::cout << "[UI] Battle Damage Assessment started\n";
    emit bdaAssessmentStarted();
    emit statusUpdated("Running BDA...");
}

void GCSBackendInterface::onBDAComplete(const QString& health_status) {
    std::cout << "[UI] BDA complete: " << health_status.toStdString() << "\n";
    emit bdaResult(health_status);
    emit statusUpdated("BDA: " + health_status);
}

void GCSBackendInterface::onRTBInitiated(const QString& reason) {
    std::cout << "[UI] RTB initiated: " << reason.toStdString() << "\n";
    emit rtbInitiated(reason);
    emit statusUpdated("Returning to Base - " + reason);
}

void GCSBackendInterface::onRecoveryPlanUpdated(const QString& plan) {
    std::cout << "[UI] Recovery plan: " << plan.toStdString() << "\n";
    emit recoveryPlanUpdated(plan);
}

void GCSBackendInterface::onLandingDetected() {
    std::cout << "[UI] Landing detected!\n";
    emit landingDetected();
    emit statusUpdated("LANDED");
}

void GCSBackendInterface::onMissionCompleted() {
    std::cout << "[UI] Mission completed!\n";
    emit missionCompleted();
    emit statusUpdated("MISSION COMPLETE");
}
