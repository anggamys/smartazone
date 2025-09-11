#include "ble_manager.h"

// buffer untuk log aman (diakses dari loop)
char BLEManager::logBuffer[64] = {0};  // Fixed size buffer instead of String
bool BLEManager::hasLog = false;

// buffer HR callback
static bool* hrFlagPtr = nullptr;
static int*  hrBufferPtr = nullptr;

void BLEManager::setHRAvailableFlag(bool* flag, int* buffer) {
    hrFlagPtr   = flag;
    hrBufferPtr = buffer;
}

int BLEManager::lastHeartRate = -1;

BLEManager::BLEManager(const char* targetAddress, uint32_t scanTime)
    : targetAddress(targetAddress),
      scanTime(scanTime),
      deviceConnected(false),
      reconnecting(false),
      lastReconnectAttempt(0),
      pClient(nullptr) {}

void BLEManager::pushLog(const char* msg) {
    // Use a smaller buffer to save memory (64 chars instead of potentially larger String)
    static char logBuffer[64] = {0};
    strncpy(logBuffer, msg, 63);
    logBuffer[63] = '\0';
    hasLog = true;
}

bool BLEManager::popLog(String& out) {
    if (hasLog) {
        out = String(logBuffer);
        hasLog = false;
        return true;
    }
    return false;
}

// ---- callbacks ----
void BLEManager::MyClientCallback::onConnect(BLEClient* pClient) {
    parent->deviceConnected = true;
    parent->reconnecting    = false;
    BLEManager::pushLog("[BLE] Connected to device!");
}

void BLEManager::MyClientCallback::onDisconnect(BLEClient* pClient) {
    parent->deviceConnected = false;
    parent->reconnecting    = true;
    BLEManager::pushLog("[BLE] Disconnected, will attempt reconnect...");
}

// ---- core BLE ----
bool BLEManager::connectToServer(BLEAddress address) {
    BLEManager::pushLog("[BLE] Connecting to server...");

    if (pClient != nullptr && pClient->isConnected()) {
        BLEManager::pushLog("[BLE] Already connected.");
        return true;
    }

    if (pClient != nullptr) {
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
    }

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback(this));

    if (pClient->connect(address)) {
        BLEManager::pushLog("[BLE] Successfully connected to server.");
        deviceConnected = true;
        reconnecting    = false;
        return true;
    }

    BLEManager::pushLog("[BLE] Failed to connect to server.");
    deviceConnected = false;
    return false;
}

bool BLEManager::connectToService(const BLEUUID& serviceUUID) {
    if (!deviceConnected || pClient == nullptr) {
        BLEManager::pushLog("[BLE] Cannot connect to service.");
        return false;
    }

    BLERemoteService* pService = pClient->getService(serviceUUID);
    if (!pService) {
        BLEManager::pushLog("[BLE] Service not found.");
        return false;
    }

    BLEManager::pushLog("[BLE] Connected to service.");
    return true;
}

bool BLEManager::enableNotify(const BLEUUID& serviceUUID, const BLEUUID& charUUID) {
    if (!deviceConnected || pClient == nullptr) {
        BLEManager::pushLog("[BLE] Not connected to server, cannot enable notify.");
        return false;
    }

    BLERemoteService* pService = pClient->getService(serviceUUID);
    if (!pService) {
        BLEManager::pushLog("[BLE] Service not found.");
        return false;
    }

    BLERemoteCharacteristic* pChar = pService->getCharacteristic(charUUID);
    if (!pChar) {
        BLEManager::pushLog("[BLE] Characteristic not found.");
        return false;
    }

    if (pChar->canNotify()) {
        pChar->registerForNotify(heartRateNotifyCallback);
        BLEManager::pushLog("[BLE] Notifications enabled.");
        return true;
    }

    BLEManager::pushLog("[BLE] Characteristic cannot notify.");
    return false;
}

void BLEManager::scanDevices() {
    BLEManager::pushLog("[BLE] Scanning for devices...");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    
    // Optimize scan parameters for better performance and lower power
    pBLEScan->setInterval(100);  // Faster scan interval
    pBLEScan->setWindow(50);     // Shorter scan window
    
    BLEScanResults results = pBLEScan->start(scanTime); // Use class member instead of hardcoded 5

    bool deviceFound = false;
    for (int i = 0; i < results.getCount() && !deviceFound; i++) {
        BLEAdvertisedDevice dev = results.getDevice(i);
        if (dev.getAddress().toString() == targetAddress) {
            BLEManager::pushLog("[BLE] Target device found!");
            deviceFound = true;
            connectToServer(dev.getAddress());
        }
    }
    
    if (!deviceFound) {
        BLEManager::pushLog("[BLE] Target device not found");
    }
    
    // Clear scan results to free memory
    pBLEScan->clearResults();
}

void BLEManager::begin() {
    BLEManager::pushLog("[BLE] Initializing BLE...");
    BLEDevice::init("");
    scanDevices();
}

bool BLEManager::tryReconnect() {
    if (!deviceConnected && reconnecting &&
        millis() - lastReconnectAttempt >= reconnectInterval) {
        lastReconnectAttempt = millis();
        BLEManager::pushLog("[BLE] Attempting reconnect...");
        scanDevices();
        return true;
    }
    return false;
}

// ---- HR notify callback ----
void BLEManager::heartRateNotifyCallback(BLERemoteCharacteristic*,
                                         uint8_t* pData, size_t length, bool) {
    if (length > 1) {
        bool is16bit = pData[0] & 0x01;
        uint16_t hrValue = is16bit ? (pData[1] | (pData[2] << 8)) : pData[1];
        lastHeartRate = hrValue;

        if (hrFlagPtr && hrBufferPtr) {
            *hrBufferPtr = hrValue;
            *hrFlagPtr   = true;
        }
    }
}
