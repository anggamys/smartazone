#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLEAdvertisedDevice.h>
#include "config.h"

// Forward declaration to avoid circular dependency
class DisplayManager;

enum class BLEConnectionState {
    DISCONNECTED,
    SCANNING,
    CONNECTING,
    CONNECTED,
    ERROR
};

// Forward declaration of BLEManager for callbacks
class BLEManager;

// BLE Client Callbacks
class BLEManagerClientCallbacks : public ::BLEClientCallbacks {
private:
    BLEManager* manager;

public:
    explicit BLEManagerClientCallbacks(BLEManager* mgr) : manager(mgr) {}
    void onConnect(BLEClient* client) override;
    void onDisconnect(BLEClient* client) override;
};

// BLE Scan Callbacks  
class BLEScanCallbacks : public BLEAdvertisedDeviceCallbacks {
private:
    BLEManager* manager;
    String targetAddress;

public:
    BLEScanCallbacks(BLEManager* mgr, const String& address) : manager(mgr), targetAddress(address) {}
    void onResult(BLEAdvertisedDevice advertisedDevice) override;
};

// Main BLE Manager Class
class BLEManager {
private:
    BLEClient* client;
    BLEScan* scanner;
    DisplayManager* display;
    BLEConnectionState connectionState;
    String targetDeviceAddress;
    unsigned long lastConnectionAttempt;
    int connectionRetries;
    
    // BLE Service and Characteristic UUIDs
    static const char* SERVICE_UUID;
    static const char* CHARACTERISTIC_UUID;
    static const char* NOTIFY_CHARACTERISTIC_UUID;
    
    // Callback objects
        BLEManagerClientCallbacks* clientCallbacks;
        BLEScanCallbacks* scanCallbacks;
        
        // Remote service and characteristics
        BLERemoteService* remoteService;
        BLERemoteCharacteristic* remoteCharacteristic;
        BLERemoteCharacteristic* notifyCharacteristic;
    
    // Constants
    static const int MAX_CONNECTION_RETRIES = 3;
    static const unsigned long RECONNECTION_DELAY = 5000;
    static const int SCAN_TIME = 5; // seconds

    // Private helper methods
    void cleanup();
    
    // Static callback for notifications (required by BLE library)
    static void notifyCallback(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify);
    
    // Static instance pointer for callback access
    static BLEManager* instance;

public:
    explicit BLEManager(DisplayManager* displayMgr);
    ~BLEManager();

    // Core functionality
    bool initialize();
    bool connectToDevice(const String& deviceAddress);
    bool disconnect();
    void handleReconnection();
    
    // State management
    BLEConnectionState getConnectionState() const { return connectionState; }
    void setConnectionState(BLEConnectionState state);
    bool isConnected() const { return connectionState == BLEConnectionState::CONNECTED; }
    
    // Device management
    void startScan();
    void stopScan();
    bool isTargetDevice(const String& address) const;
    
    // Data operations
    bool readSensorData();
    bool subscribeToNotifications();
    void handleNotification(uint8_t* data, size_t length);
    
    // Status and debugging
    void printConnectionInfo() const;
    String getConnectionStateString() const;

    // Friend classes for callback access
        friend class BLEManagerClientCallbacks;
        friend class BLEScanCallbacks;
};

#endif // BLE_MANAGER_H
