#pragma once

#include <thread>
#include <memory>
#include <string>
#include <atomic>

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
    std::string mission_file_;
};
