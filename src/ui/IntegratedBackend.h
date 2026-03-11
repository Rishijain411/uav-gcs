#pragma once

#include <thread>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>

class GCSBackendInterface;

// Integrated GCS Backend running in background thread
class IntegratedBackend {
public:
    IntegratedBackend(GCSBackendInterface* ui_interface);
    ~IntegratedBackend();
    
    void start();
    void stop();
    void loadMission(const std::string& mission_file);
    
    // Command methods
    void sendArmCommand();
    void sendDisarmCommand();
    void sendTakeoffCommand();
    void sendAbortCommand();
    void sendEngageCommand();
    void sendRtlCommand();
    void sendLandCommand();
    void updateFailsafeRules(int comms, int battery, int gps);
    GCSBackendInterface* getInterface() const { return ui_interface_; }
    
private:
    void runBackendLoop();
    
    GCSBackendInterface* ui_interface_;
    std::unique_ptr<std::thread> backend_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> mission_load_requested_;
    std::atomic<bool> arm_requested_;
    std::atomic<bool> disarm_requested_;
    std::atomic<bool> takeoff_requested_;
    std::atomic<bool> abort_requested_;
    std::atomic<bool> engage_requested_;
    std::atomic<bool> rtl_requested_;
    std::atomic<bool> land_requested_;
    
    // Thread-safe mission file access
    std::mutex mission_file_mutex_;
    std::string mission_file_;
    // below is for fail safe 
    std::atomic<bool> failsafe_update_pending_{false};
    std::atomic<int> pending_comms_loss_{-1};
    std::atomic<int> pending_low_battery_{-1};
    std::atomic<int> pending_gps_jamming_{-1};
};
