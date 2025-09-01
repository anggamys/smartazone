#include <Arduino.h>
#include "config.h"
#include "display_manager.h"
#include "ble_manager.h"
#include "sensor_data.h"

// Global instances
DisplayManager displayManager;
BLEManager* bleManager = nullptr;
SensorData currentSensorData;

// Application state
unsigned long lastStatusUpdate = 0;
unsigned long lastSensorRead = 0;

// Function declarations
void initializeSystem();
void updateDisplay();
void handleBLEOperations();
void displaySensorData();
void displaySystemStatus();

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Serial.println(F("\n=== ESP32 BLE Health Monitor ==="));
    Serial.println(F("Initializing system..."));
    
    initializeSystem();
}

void loop() {
    unsigned long currentTime = millis();
    
    // Handle BLE operations
    handleBLEOperations();
    
    // Update display periodically
    if (currentTime - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
        updateDisplay();
        lastStatusUpdate = currentTime;
    }
    
    // Read sensor data if connected
    if (bleManager && bleManager->isConnected() && 
        currentTime - lastSensorRead >= SENSOR_READ_INTERVAL) {
        
        if (bleManager->readSensorData()) {
            displaySensorData();
        }
        lastSensorRead = currentTime;
    }
    
    delay(MAIN_LOOP_DELAY);
}

void initializeSystem() {
    // Initialize display first
    if (!displayManager.initialize()) {
        Serial.println(F("[SYSTEM] Display initialization failed - continuing without display"));
    } else {
        displayManager.printCenteredText("ESP32 BLE", 15);
        displayManager.printCenteredText("Health Monitor", 35);
        delay(2000);
    }
    
    // Initialize BLE manager
    bleManager = new BLEManager(&displayManager);
    
    if (!bleManager->initialize()) {
        Serial.println(F("[SYSTEM] BLE initialization failed"));
        if (displayManager.isReady()) {
            displayManager.printCenteredText("BLE Init Failed", 25);
        }
        return;
    }
    
    // Start connection process
    Serial.println(F("[SYSTEM] Starting BLE connection process..."));
    if (displayManager.isReady()) {
        displayManager.printCenteredText("Starting BLE...", 25);
    }
    
    bleManager->connectToDevice(BLE_DEVICE_MAC);
}

void updateDisplay() {
    if (!displayManager.isReady()) return;
    
    if (bleManager) {
        displaySystemStatus();
    }
}

void handleBLEOperations() {
    if (!bleManager) return;
    
    // Handle automatic reconnection
    bleManager->handleReconnection();
    
    // Print connection info periodically for debugging
    static unsigned long lastInfoPrint = 0;
    if (millis() - lastInfoPrint > 10000) {
        bleManager->printConnectionInfo();
        lastInfoPrint = millis();
    }
}

void displaySensorData() {
    if (!displayManager.isReady() || !currentSensorData.hasValidData()) return;
    
    displayManager.clear();
    
    String line1 = "HR: " + String(currentSensorData.heartRate) + " bpm";
    String line2 = "Steps: " + String(currentSensorData.stepCount);
    
    displayManager.printTwoLines(line1, line2);
    
    // Show battery level as progress bar
    if (currentSensorData.batteryValid) {
        displayManager.drawProgressBar(10, 50, 100, 8, 
                                     currentSensorData.batteryLevel, true);
    }
}

void displaySystemStatus() {
    if (!bleManager) return;
    
    String statusText = bleManager->getConnectionStateString();
    displayManager.printCenteredText(statusText, 25);
    
    // Show connection indicator
    displayManager.drawConnectionStatus(bleManager->isConnected());
}
