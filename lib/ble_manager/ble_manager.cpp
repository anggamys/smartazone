#include "ble_manager.h"
#include "display_manager.h"

// Static member definitions
const char* BLEManager::SERVICE_UUID = "12345678-1234-1234-1234-123456789abc";
const char* BLEManager::CHARACTERISTIC_UUID = "87654321-4321-4321-4321-cba987654321";
const char* BLEManager::NOTIFY_CHARACTERISTIC_UUID = "11111111-2222-3333-4444-555555555555";
BLEManager* BLEManager::instance = nullptr;

// BLE Client Callbacks Implementation
void BLEManagerClientCallbacks::onConnect(BLEClient* client) {
    Serial.println(F("[BLE] Device connected successfully"));
    manager->setConnectionState(BLEConnectionState::CONNECTED);
    
    if (manager->display && manager->display->isReady()) {
        manager->display->printCenteredText("BLE Connected", 20);
        manager->display->drawConnectionStatus(true);
    }
}

void BLEManagerClientCallbacks::onDisconnect(BLEClient* client) {
    Serial.println(F("[BLE] Device disconnected"));
    manager->setConnectionState(BLEConnectionState::DISCONNECTED);
    
    if (manager->display && manager->display->isReady()) {
        manager->display->printCenteredText("BLE Disconnected", 20);
        manager->display->drawConnectionStatus(false);
    }
}

// BLE Scan Callbacks Implementation
void BLEScanCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    String deviceAddress = advertisedDevice.getAddress().toString().c_str();
    Serial.printf("[BLE] Found device: %s\n", deviceAddress.c_str());
    
    if (manager->isTargetDevice(deviceAddress)) {
        Serial.println(F("[BLE] Target device found, stopping scan"));
        manager->stopScan();
        
        // Connect to the found device
        BLEAddress address = advertisedDevice.getAddress();
        if (manager->display && manager->display->isReady()) {
            manager->display->printCenteredText("Device Found!", 15);
            manager->display->printCenteredText("Connecting...", 35);
        }
        
        manager->setConnectionState(BLEConnectionState::CONNECTING);
        manager->client->connect(address);
    }
}

// Static notification callback implementation
void BLEManager::notifyCallback(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify) {
    if (instance) {
        instance->handleNotification(data, length);
    }
}

// BLE Manager Implementation
BLEManager::BLEManager(DisplayManager* displayMgr) 
    : client(nullptr),
      scanner(nullptr),
      display(displayMgr),
      connectionState(BLEConnectionState::DISCONNECTED),
      lastConnectionAttempt(0),
      connectionRetries(0),
      clientCallbacks(nullptr),
      scanCallbacks(nullptr),
      remoteService(nullptr),
      remoteCharacteristic(nullptr),
      notifyCharacteristic(nullptr) {
    instance = this;
}

BLEManager::~BLEManager() {
    cleanup();
    instance = nullptr;
}

void BLEManager::cleanup() {
    if (client && client->isConnected()) {
        client->disconnect();
    }
    
    if (scanner) {
        scanner->stop();
    }
    
    // Clean up callback objects
    delete clientCallbacks;
    clientCallbacks = nullptr;
    
    delete scanCallbacks;
    scanCallbacks = nullptr;
    
    // Reset pointers
    remoteService = nullptr;
    remoteCharacteristic = nullptr;
    notifyCharacteristic = nullptr;
}

bool BLEManager::initialize() {
    Serial.println(F("[BLE] Initializing BLE device..."));
    
    try {
        BLEDevice::init("SmartAzone");  // Fixed: Added device name
        
        // Create BLE client
        client = BLEDevice::createClient();
        if (!client) {
            Serial.println(F("[BLE] Failed to create client"));
            setConnectionState(BLEConnectionState::ERROR);
            return false;
        }
        
        // Create and set client callbacks
        clientCallbacks = new BLEManagerClientCallbacks(this);
        client->setClientCallbacks(clientCallbacks);
        
        // Create scanner
        scanner = BLEDevice::getScan();
        if (!scanner) {
            Serial.println(F("[BLE] Failed to create scanner"));
            setConnectionState(BLEConnectionState::ERROR);
            return false;
        }
        
        Serial.println(F("[BLE] Initialization successful"));
        return true;
        
    } catch (const std::exception& e) {
        Serial.printf("[BLE] Initialization error: %s\n", e.what());
        setConnectionState(BLEConnectionState::ERROR);
        return false;
    }
}

void BLEManager::setConnectionState(BLEConnectionState state) {
    connectionState = state;
    
    if (display && display->isReady()) {
        display->drawConnectionStatus(state == BLEConnectionState::CONNECTED);
    }
}

bool BLEManager::connectToDevice(const String& deviceAddress) {
    if (!client || !scanner) {
        Serial.println(F("[BLE] BLE not properly initialized"));
        return false;
    }
    
    targetDeviceAddress = deviceAddress;
    
    if (display && display->isReady()) {
        display->printCenteredText("Scanning for", 15);
        display->printCenteredText("Smart Watch...", 35);
    }
    
    Serial.printf("[BLE] Starting scan for device: %s\n", deviceAddress.c_str());
    setConnectionState(BLEConnectionState::SCANNING);
    
    startScan();
    return true;
}

void BLEManager::startScan() {
    if (!scanner) return;
    
    // Clean up previous scan callbacks
    delete scanCallbacks;
    scanCallbacks = new BLEScanCallbacks(this, targetDeviceAddress);
    
    scanner->setAdvertisedDeviceCallbacks(scanCallbacks);
    scanner->setActiveScan(true);
    scanner->setInterval(100);
    scanner->setWindow(99);
    
    // Start async scan
    scanner->start(SCAN_TIME, false);  // Fixed: Use consistent constant name
}

void BLEManager::stopScan() {
    if (scanner) {
        scanner->stop();
    }
}

bool BLEManager::isTargetDevice(const String& address) const {
    return address.equalsIgnoreCase(targetDeviceAddress);
}

bool BLEManager::disconnect() {
    if (client && client->isConnected()) {
        client->disconnect();
        return true;
    }
    return false;
}

void BLEManager::handleReconnection() {
    if (connectionState == BLEConnectionState::DISCONNECTED && 
        millis() - lastConnectionAttempt > RECONNECTION_DELAY &&
        connectionRetries < MAX_CONNECTION_RETRIES) {
        
        Serial.println(F("[BLE] Attempting reconnection..."));
        lastConnectionAttempt = millis();
        connectionRetries++;
        
        if (display && display->isReady()) {
            display->printCenteredText("Reconnecting...", 25);
        }
        
        connectToDevice(targetDeviceAddress);
    } else if (connectionRetries >= MAX_CONNECTION_RETRIES) {
        setConnectionState(BLEConnectionState::ERROR);
        if (display && display->isReady()) {
            display->printCenteredText("Connection Failed", 25);
        }
    }
}

String BLEManager::getConnectionStateString() const {
    switch (connectionState) {
        case BLEConnectionState::DISCONNECTED: return "Disconnected";
        case BLEConnectionState::SCANNING: return "Scanning";
        case BLEConnectionState::CONNECTING: return "Connecting";
        case BLEConnectionState::CONNECTED: return "Connected";
        case BLEConnectionState::ERROR: return "Error";
        default: return "Unknown";
    }
}

void BLEManager::printConnectionInfo() const {
    Serial.printf("[BLE] State: %s, Retries: %d\n", 
                  getConnectionStateString().c_str(), connectionRetries);
}

bool BLEManager::readSensorData() {
    if (!isConnected()) {
        Serial.println(F("[BLE] Cannot read data - not connected"));
        return false;
    }
    
    try {
        // Get the remote service
        remoteService = client->getService(SERVICE_UUID);
        if (!remoteService) {
            Serial.println(F("[BLE] Failed to find service"));
            return false;
        }
        
        // Get the characteristic
        remoteCharacteristic = remoteService->getCharacteristic(CHARACTERISTIC_UUID);
        if (!remoteCharacteristic) {
            Serial.println(F("[BLE] Failed to find characteristic"));
            return false;
        }
        
        if (remoteCharacteristic->canRead()) {
            std::string value = remoteCharacteristic->readValue();
            Serial.printf("[BLE] Read value: %s\n", value.c_str());
            return true;
        }
    } catch (const std::exception& e) {
        Serial.printf("[BLE] Read error: %s\n", e.what());
    }
    
    return false;
}

bool BLEManager::subscribeToNotifications() {
    if (!isConnected()) {
        Serial.println(F("[BLE] Cannot subscribe - not connected"));
        return false;
    }
    
    try {
        // Get the remote service
        remoteService = client->getService(SERVICE_UUID);
        if (!remoteService) {
            Serial.println(F("[BLE] Failed to find service for notifications"));
            return false;
        }
        
        // Get the notification characteristic
        notifyCharacteristic = remoteService->getCharacteristic(NOTIFY_CHARACTERISTIC_UUID);
        if (!notifyCharacteristic) {
            Serial.println(F("[BLE] Failed to find notification characteristic"));
            return false;
        }
        
        if (notifyCharacteristic->canNotify()) {
            notifyCharacteristic->registerForNotify(notifyCallback);
            Serial.println(F("[BLE] Subscribed to notifications"));
            return true;
        }
    } catch (const std::exception& e) {
        Serial.printf("[BLE] Notification subscription error: %s\n", e.what());
    }
    
    return false;
}

void BLEManager::handleNotification(uint8_t* data, size_t length) {
    Serial.printf("[BLE] Notification received: %d bytes\n", length);
    
    // Process the notification data here
    for (size_t i = 0; i < length; i++) {
        Serial.printf("0x%02X ", data[i]);
    }
    Serial.println();
}

