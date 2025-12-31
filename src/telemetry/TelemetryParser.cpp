#include "TelemetryParser.h"
#include "telemetry/TelemetryData.h"

#include <iostream>

extern "C" {
#include "mavlink/common/mavlink.h"
}

static mavlink_message_t msg;
static mavlink_status_t status;

TelemetryParser::TelemetryParser(
    TelemetryData& data,
    StateManager& stateMgr)
    : telemetry(data), stateManager(stateMgr) {}

void TelemetryParser::parse(uint8_t byte) {

    if (mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &status)) {

        // ✅ NEW: update link activity timestamp
        telemetry.last_mavlink_rx_time =
            std::chrono::steady_clock::now();

        switch (msg.msgid) {

        // ---------- HEARTBEAT ----------
        case MAVLINK_MSG_ID_HEARTBEAT: {
            telemetry.system_id = msg.sysid;
            telemetry.component_id = msg.compid;
            telemetry.heartbeat_received = true;
            telemetry.last_heartbeat_time =
                std::chrono::steady_clock::now();

            if (stateManager.getState() ==
                SystemState::DISCONNECTED) {
                stateManager.setState(SystemState::CONNECTED);
            }

            std::cout << "[HEARTBEAT] Vehicle detected (SysID "
                      << static_cast<int>(msg.sysid) << ")"
                      << std::endl;
            break;
        }

        // ---------- SYS_STATUS (battery) ----------
        case MAVLINK_MSG_ID_SYS_STATUS: {
            mavlink_sys_status_t sys;
            mavlink_msg_sys_status_decode(&msg, &sys);

           telemetry.battery_ok =
                (sys.battery_remaining > 20) ||
                (sys.battery_remaining == -1);

            telemetry.battery_received = true;

            break;
        }
        // ---------- ESTIMATOR_STATUS (EKF) ----------
        case MAVLINK_MSG_ID_ESTIMATOR_STATUS: {
            mavlink_estimator_status_t est;
            mavlink_msg_estimator_status_decode(&msg, &est);

            constexpr uint16_t EST_ATTITUDE_OK       = 1 << 0;
            constexpr uint16_t EST_VELOCITY_HORIZ_OK = 1 << 1;

            bool attitude_ok =
                (est.flags & EST_ATTITUDE_OK) != 0;

            bool velocity_ok =
                (est.flags & EST_VELOCITY_HORIZ_OK) != 0;

            telemetry.ekf_ok = attitude_ok && velocity_ok;
            telemetry.ekf_received = true;
            break;
        }






        // ---------- COMMAND_ACK (Phase 4.2) ----------
        case MAVLINK_MSG_ID_COMMAND_ACK: {

            // Do NOT overwrite an unconsumed ACK
            if (telemetry.last_command_ack.valid)
                break;

            mavlink_command_ack_t ack;
            mavlink_msg_command_ack_decode(&msg, &ack);

            telemetry.last_command_ack.command_id = ack.command;
            telemetry.last_command_ack.result = ack.result;
            telemetry.last_command_ack.valid = true;

            std::cout << "[ACK] CMD="
                      << ack.command
                      << " RESULT="
                      << int(ack.result)
                      << std::endl;

            break;
        }

        default:
            break;
        }
    }
}
