// Qt UI with integrated GCS Backend

#include "ui/MainWindow_QGC.h"
#include "ui/GCSBackendInterface.h"
#include "ui/IntegratedBackend.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Create main window
    MainWindow window;
    
    // Create backend interface
    GCSBackendInterface* backend_interface = new GCSBackendInterface(&app);
    
    // Create integrated backend
    IntegratedBackend* backend = new IntegratedBackend(backend_interface);
    
    // Connect backend signals to UI slots
    QObject::connect(backend_interface, &GCSBackendInterface::connectionStatusChanged,
                    &window, &MainWindow::onConnectionStatusChanged);
    QObject::connect(backend_interface, &GCSBackendInterface::missionStateChanged,
                    &window, &MainWindow::onMissionStateChanged);
    QObject::connect(backend_interface, &GCSBackendInterface::positionUpdated,
                    &window, &MainWindow::updateTelemetryDisplay);
    QObject::connect(backend_interface, &GCSBackendInterface::armStateChanged,
                    &window, &MainWindow::updateArmState);
    QObject::connect(backend_interface, &GCSBackendInterface::modeChanged,
                    &window, &MainWindow::updateFlightMode);
    QObject::connect(backend_interface, &GCSBackendInterface::batteryUpdated,
                    &window, &MainWindow::updateBatteryDisplay);
    QObject::connect(backend_interface, &GCSBackendInterface::missionUploadProgress,
                    &window, &MainWindow::updateMissionUploadProgress);
    QObject::connect(backend_interface, &GCSBackendInterface::missionCurrentWaypoint,
                    &window, &MainWindow::updateCurrentWaypoint);
    QObject::connect(backend_interface, &GCSBackendInterface::errorOccurred,
                    &window, &MainWindow::displayError);
    QObject::connect(backend_interface, &GCSBackendInterface::missionUploadSuccess,
                    &window, &MainWindow::onMissionUploadSuccess);
    QObject::connect(backend_interface, &GCSBackendInterface::statusUpdated,
                    &window, &MainWindow::onStatusUpdated);
    QObject::connect(backend_interface, &GCSBackendInterface::healthStatusUpdated,
                    &window, &MainWindow::onHealthStatusUpdated);
    
    // Recovery & RTB connections (New)
    QObject::connect(backend_interface, &GCSBackendInterface::commsLossDetected,
                    &window, &MainWindow::onCommsLoss);
    QObject::connect(backend_interface, &GCSBackendInterface::bdaAssessmentStarted,
                    &window, &MainWindow::onBDAStarted);
    QObject::connect(backend_interface, &GCSBackendInterface::bdaResult,
                    &window, &MainWindow::onBDAResult);
    QObject::connect(backend_interface, &GCSBackendInterface::rtbInitiated,
                    &window, &MainWindow::onRTBStarted);
    QObject::connect(backend_interface, &GCSBackendInterface::landingDetected,
                    &window, &MainWindow::onLandingDetected);
    QObject::connect(backend_interface, &GCSBackendInterface::missionCompleted,
                    &window, &MainWindow::onMissionCompleted);
    QObject::connect(backend_interface, &GCSBackendInterface::payloadArmingCountdown,
                &window, &MainWindow::onPayloadArmingCountdown);
    // Connection for the initial "Request" alert (Fixed 'this' error)
    QObject::connect(backend_interface, &GCSBackendInterface::payloadArmingRequested,
                    &window, &MainWindow::onPayloadArmingRequested);
    // Set backend pointer in MainWindow for direct control
    window.setBackend(backend);
    
    // Start backend thread
    backend->start();
    
    window.show();
    
    int result = app.exec();
    
    // Cleanup
    backend->stop();
    delete backend;
    delete backend_interface;
    
    return result;
}
