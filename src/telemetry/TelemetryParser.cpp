#include "telemetry/TelemetryParser.h"
#include "telemetry/TelemetryData.h"
#include "core/StateManager.h"

#include <iostream>
#include <chrono>
#include <cstring>

extern "C" {
#include "mavlink/common/mavlink.h"
}

static mavlink_message_t msg;
static mavlink_status_t mav_status;

// 🔒 MUST MATCH GCS HEARTBEAT SYSID
static constexpr uint8_t GCS_SYS_ID = 50;

TelemetryParser::TelemetryParser(
    TelemetryData& data,
    StateManager& stateMgr)
    : telemetry(data), stateManager(stateMgr) {}

void TelemetryParser::parse(uint8_t byte) {

    if (!mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &mav_status))
        return;

    // Any MAVLink message means link is alive
    telemetry.last_mavlink_rx_time = std::chrono::steady_clock::now();

    switch (msg.msgid) {

    // ================= HEARTBEAT =================
    case MAVLINK_MSG_ID_HEARTBEAT: {
        mavlink_heartbeat_t hb;
        mavlink_msg_heartbeat_decode(&msg, &hb);

        // ❌ Ignore our own GCS heartbeat
        if (msg.sysid == GCS_SYS_ID)
            break;

        telemetry.system_id = msg.sysid;
        telemetry.component_id = msg.compid;
        telemetry.heartbeat_received = true;
        telemetry.last_heartbeat_time =
            std::chrono::steady_clock::now();

        // ---- ARM STATE ----
        telemetry.arm_state =
            (hb.base_mode & MAV_MODE_FLAG_SAFETY_ARMED)
                ? ArmState::ARMED
                : ArmState::DISARMED;

        // ---- FAILSAFE ----
        telemetry.in_failsafe =
            (hb.system_status == MAV_STATE_CRITICAL ||
             hb.system_status == MAV_STATE_EMERGENCY);

        if (telemetry.in_failsafe) {
            telemetry.last_block_reason =
                CommandBlockReason::FAILSAFE_ACTIVE;
        }

        if (stateManager.getState() == SystemState::DISCONNECTED) {
            stateManager.setState(SystemState::CONNECTED);
        }

        std::cout << "[HEARTBEAT] Vehicle detected (SysID "
                  << int(msg.sysid) << ")" << std::endl;
        break;
    }

    // ================= BATTERY =================
    case MAVLINK_MSG_ID_SYS_STATUS: {
        mavlink_sys_status_t sys;
        mavlink_msg_sys_status_decode(&msg, &sys);

        telemetry.battery_ok =
            (sys.battery_remaining > 20) ||
            (sys.battery_remaining == -1);

        telemetry.battery_received = true;

        if (!telemetry.battery_ok &&
            telemetry.last_block_reason != CommandBlockReason::FAILSAFE_ACTIVE) {

            telemetry.last_block_reason =
                CommandBlockReason::BATTERY_LOW;
        }
        break;
    }

    // ================= EKF =================
    case MAVLINK_MSG_ID_ESTIMATOR_STATUS: {
        mavlink_estimator_status_t est;
        mavlink_msg_estimator_status_decode(&msg, &est);

        constexpr uint16_t EST_ATT_OK = 1 << 0;
        constexpr uint16_t EST_VEL_OK = 1 << 1;

        telemetry.ekf_ok =
            (est.flags & EST_ATT_OK) &&
            (est.flags & EST_VEL_OK);

        telemetry.ekf_received = true;

        if (!telemetry.ekf_ok &&
            telemetry.last_block_reason != CommandBlockReason::FAILSAFE_ACTIVE) {

            telemetry.last_block_reason =
                CommandBlockReason::EKF_NOT_READY;
        }
        break;
    }

    // ================= LANDED / AIRBORNE =================
    case MAVLINK_MSG_ID_EXTENDED_SYS_STATE: {
        mavlink_extended_sys_state_t ext;
        mavlink_msg_extended_sys_state_decode(&msg, &ext);

        telemetry.extended_state_received = true;

        switch (ext.landed_state) {
        case MAV_LANDED_STATE_ON_GROUND:
            telemetry.flight_phase = FlightPhase::ON_GROUND;
            break;

        case MAV_LANDED_STATE_TAKEOFF:
            telemetry.flight_phase = FlightPhase::TAKING_OFF;
            break;

        case MAV_LANDED_STATE_IN_AIR:
            telemetry.flight_phase = FlightPhase::IN_AIR;
            break;

        case MAV_LANDED_STATE_LANDING:
            telemetry.flight_phase = FlightPhase::LANDING;
            break;

        default:
            telemetry.flight_phase = FlightPhase::UNKNOWN;
            break;
        }
        break;
    }

    // ================= COMMAND ACK =================
    case MAVLINK_MSG_ID_COMMAND_ACK: {
        if (telemetry.last_command_ack.valid)
            break;

        mavlink_command_ack_t ack;
        mavlink_msg_command_ack_decode(&msg, &ack);

        telemetry.last_command_ack.command_id = ack.command;
        telemetry.last_command_ack.result = ack.result;
        telemetry.last_command_ack.valid = true;

        telemetry.last_block_reason = CommandBlockReason::NONE;

        std::cout << "[ACK] CMD=" << ack.command
                  << " RESULT=" << int(ack.result) << std::endl;
        break;
    }

    // ================= STATUSTEXT (LOGGING ONLY) =================
    case MAVLINK_MSG_ID_STATUSTEXT: {
        mavlink_statustext_t st;
        mavlink_msg_statustext_decode(&msg, &st);

        std::strncpy(
            telemetry.last_status_text,
            reinterpret_cast<char*>(st.text),
            sizeof(telemetry.last_status_text) - 1
        );

        telemetry.last_status_text[
            sizeof(telemetry.last_status_text) - 1] = '\0';

        std::cout << "[PX4] "
                  << telemetry.last_status_text
                  << std::endl;
        break;
    }

    default:
        break;
    }
}
