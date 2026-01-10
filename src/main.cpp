#include <iostream>
#include <chrono>

using namespace std;

#include "comm/UdpTransport.h"
#include "telemetry/TelemetryParser.h"
#include "telemetry/TelemetryData.h"
#include "core/StateManager.h"
#include "command/CommandManager.h"
#include "command/MavlinkCommandSender.h"
#include "comm/GcsHeartbeat.h"

constexpr int HEARTBEAT_TIMEOUT_MS = 2000;

int main() {

    UdpTransport udp;
    TelemetryData telemetry;
    StateManager stateManager;
    CommandManager commandManager;
    TelemetryParser parser(telemetry, stateManager);

    if (!udp.start(14550)) {
        cerr << "Failed to start UDP transport\n";
        return -1;
    }

    // ---------------- GCS Heartbeat ----------------
    GcsHeartbeat gcsHeartbeat(udp.getSocketFd());
    auto last_hb = chrono::steady_clock::now();
    auto last_failsafe_check = chrono::steady_clock::now();

    cout << "[GCS] Heartbeat sender initialized\n";

    // ---------------- Command sender ----------------
    MavlinkCommandSender* cmdSender = nullptr;
    bool sender_initialized = false;

    uint8_t buffer[2048];

    // ---------------- Mission definition ----------------
    static VehicleCommand mission[] = {
        VehicleCommand::ARM,
        VehicleCommand::SET_MODE_AUTO,
        VehicleCommand::TAKEOFF
    };

    static constexpr int MISSION_LEN =
        sizeof(mission) / sizeof(mission[0]);

    static int mission_step = 0;

    // ================= MAIN LOOP =================
    while (true) {

        auto now = chrono::steady_clock::now();

        // ---------- Send GCS heartbeat ----------
        if (chrono::duration_cast<chrono::seconds>(now - last_hb).count() >= 1) {
            gcsHeartbeat.send();
            last_hb = now;
        }

        // ---------- Receive MAVLink ----------
        int len = udp.receive(buffer, sizeof(buffer));
        if (len > 0) {
            for (int i = 0; i < len; i++)
                parser.parse(buffer[i]);
        }

        // ---------- Command lifecycle ----------
        commandManager.update(telemetry, stateManager.getMutableState());

        // ---------- Init command sender ----------
        if (!sender_initialized && telemetry.heartbeat_received) {
            cmdSender = new MavlinkCommandSender(
                udp.getSocketFd(),
                telemetry.system_id
            );
            commandManager.setCommandSender(cmdSender);
            sender_initialized = true;
        }

        // ---------- FAILSAFE CHECK ----------
        if (telemetry.heartbeat_received &&
            chrono::duration_cast<chrono::milliseconds>(
                now - last_failsafe_check).count() >= 200) {

            last_failsafe_check = now;

            auto hb_elapsed =
                chrono::duration_cast<chrono::milliseconds>(
                    now - telemetry.last_heartbeat_time).count();

            auto link_elapsed =
                chrono::duration_cast<chrono::milliseconds>(
                    now - telemetry.last_mavlink_rx_time).count();

            if (hb_elapsed > HEARTBEAT_TIMEOUT_MS &&
                link_elapsed > HEARTBEAT_TIMEOUT_MS &&
                stateManager.getState() != SystemState::FAILSAFE) {

                stateManager.setState(SystemState::FAILSAFE);
                cout << "[FAILSAFE] MAVLink timeout\n";
            }
        }

        // ---------- Mission execution ----------
        if (!cmdSender ||
            !telemetry.isTelemetryReady() ||
            commandManager.hasActiveCommand() ||
            mission_step >= MISSION_LEN)
            continue;

        if (commandManager.requestCommand(
                mission[mission_step],
                stateManager.getState(),
                telemetry)) {

            cout << "[MISSION] Step "
                 << mission_step
                 << " issued\n";

            mission_step++;
        }
    }

    return 0;
}
