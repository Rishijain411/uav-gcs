#pragma once

#include <fstream>
#include <chrono>
#include <string>
#include <iostream>
using namespace std;

#include "command/VehicleCommand.h"
#include "mission/MissionState.h"
#include "mission/MissionEvent.h"
#include "mission/MissionAbortReason.h"

class AuditLogger {
public:
    // -------------------------------------------------
    // Existing: Decision logging
    // -------------------------------------------------
    static void logDecision(
        VehicleCommand cmd,
        mission::MissionState mission_state,
        const string& decision,
        const string& details = "")
    {
        ofstream file("mission_audit.tlog", ios::app);

        if (!file.is_open()) {
            cout << "[AUDIT] Failed to open log\n";
            return;
        }

        auto now = chrono::system_clock::now();
        auto ts = chrono::duration_cast<chrono::milliseconds>(
            now.time_since_epoch()).count();

        file << ts << ",DECISION,"
             << int(mission_state) << ","
             << int(cmd) << ","
             << decision;

        if (!details.empty())
            file << "," << details;

        file << endl;
        file.close();
    }

    // -------------------------------------------------
    // Existing: Mission transition logging
    // -------------------------------------------------
    static void logMissionTransition(
        mission::MissionState from,
        mission::MissionEvent event,
        const string& decision,
        const string& details = "")
    {
        ofstream file("mission_audit.tlog", ios::app);

        if (!file.is_open())
            return;

        auto now = chrono::system_clock::now();
        auto ts = chrono::duration_cast<chrono::milliseconds>(
            now.time_since_epoch()).count();

        file << ts << ",MISSION_TRANSITION,"
             << int(from) << ","
             << int(event) << ","
             << decision;

        if (!details.empty())
            file << "," << details;

        file << endl;
        file.close();
    }

    // -------------------------------------------------
    // 🔴 NEW (PRD): Mission Abort Reason logging
    // -------------------------------------------------
    static void logMissionAbort(
        mission::MissionState state,
        MissionAbortReason reason,
        const string& details = "")
    {
        ofstream file("mission_audit.tlog", ios::app);

        if (!file.is_open())
            return;

        auto now = chrono::system_clock::now();
        auto ts = chrono::duration_cast<chrono::milliseconds>(
            now.time_since_epoch()).count();

        file << ts << "| MISSION_ABORT |"
             << int(state) << " | "
             << int(reason);

        if (!details.empty())
            file << "," << details;

        file << endl;
        file.close();
    }
};
