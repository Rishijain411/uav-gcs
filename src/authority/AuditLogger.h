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
#include "mission/BDAResult.h"


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

        file << ts << ",MISSION_ABORT,"
            << int(state) << ","
            << int(reason);

        if (!details.empty())
            file << "," << details;

        file << endl;
        file.close();
    }
    static void logRecovery(
    mission::MissionState state,
    VehicleCommand cmd,
    const std::string& reason)
    {
        ofstream file("mission_audit.tlog", ios::app);
        if (!file.is_open()) return;

        auto now = chrono::system_clock::now();
        auto ts = chrono::duration_cast<chrono::milliseconds>(
            now.time_since_epoch()).count();

        file << ts << ",RECOVERY,"
            << int(state) << ","
            << int(cmd) << ","
            << reason << endl;

        file.close();
    }

    static void logBDAResult(
    mission::MissionState state,
    mission::BDAResult result,
    const string& details = "")
    {
        ofstream file("mission_audit.tlog", ios::app);
        if (!file.is_open()) return;

        auto now = chrono::system_clock::now();
        auto ts = chrono::duration_cast<chrono::milliseconds>(
            now.time_since_epoch()).count();

        file << ts << ",BDA_RESULT,"
            << int(state) << ","
            << int(result) << ","
            << mission::toString(result);

        if (!details.empty())
            file << "," << details;

        file << endl;

        file.close();
    }
    static void logRecoveryPlan(
    mission::MissionState state,
    int plan_code)
    {
        std::ofstream file("mission_audit.tlog", std::ios::app);
        if (!file.is_open()) return;

        auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        file << ts << ",RECOVERY_PLAN,"
            << int(state) << ","
            << plan_code
            << std::endl;
    }





};
