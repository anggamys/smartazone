#include "ble_manager.h"

// Logging helper
static void logBLE(const String& msg) {
    Serial.println("[BLE] " + msg);
}

int BLEManager::lastHeartRate = -1; // default belum ada data

BLEManager::BLEManager(const char* targetAddress)
    : targetAddress(targetAddress),
      deviceConnected(false),
      reconnecting(false),
      lastReconnectAttempt(0),
      pClient(nullptr) {}

void BLEManager::MyClientCallback::onConnect(BLEClient* pClient) {
    logBLE("Connected to device!");
    parent->deviceConnected = true;
    parent->reconnecting = false;
}

void BLEManager::MyClientCallback::onDisconnect(BLEClient* pClient) {
    logBLE("Disconnected, will attempt reconnect...");
    parent->deviceConnected = false;
    parent->reconnecting = true;
}

bool BLEManager::connectToServer(BLEAddress address) {
    logBLE("Connecting to server: " + String(address.toString().c_str()));

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback(this));

    if (pClient->connect(address)) {
        logBLE("Successfully connected to server.");
        deviceConnected = true;
        reconnecting = false;
        return true;
    } else {
        logBLE("Failed to connect to server.");
        deviceConnected = false;
        return false;
    }
}

bool BLEManager::connectToService(const BLEUUID& serviceUUID) {
    if (!deviceConnected || pClient == nullptr) {
        logBLE("Cannot connect to service, not connected to server.");
        return false;
    }

    BLERemoteService* pService = pClient->getService(serviceUUID);
    if (!pService) {
        logBLE("Service not found on server.");
        return false;
    }

    // BLEUUID::toString() is not a const method, so make a local copy to call it safely.
    BLEUUID uuid = serviceUUID;
    logBLE("Connected to service: " + String(uuid.toString().c_str()));
    return true;
}

bool BLEManager::enableNotify(const BLEUUID& serviceUUID, const BLEUUID& charUUID) {
    if (!deviceConnected || pClient == nullptr) {
        logBLE("Not connected to server, cannot enable notify.");
        return false;
    }

    BLERemoteService* pService = pClient->getService(serviceUUID);
    if (!pService) {
        logBLE("Service not found.");
        return false;
    }

    BLERemoteCharacteristic* pChar = pService->getCharacteristic(charUUID);
    if (!pChar) {
        logBLE("Characteristic not found.");
        return false;
    }

    if (pChar->canNotify()) {
        pChar->registerForNotify(heartRateNotifyCallback);
        logBLE("Notifications enabled.");
        return true;
    } else {
        logBLE("Characteristic does not support notify.");
        return false;
    }
}

void BLEManager::scanDevices() {
    logBLE("Scanning for devices...");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    BLEScanResults results = pBLEScan->start(5);

    for (int i = 0; i < results.getCount(); i++) {
        BLEAdvertisedDevice dev = results.getDevice(i);
        logBLE("Found: " + String(dev.toString().c_str()));

        if (dev.getAddress().toString() == targetAddress) {
            logBLE("Target device found!");
            connectToServer(dev.getAddress());
            break;
        }
    }
}

void BLEManager::begin() {
    logBLE("Initializing BLE...");
    BLEDevice::init("");
    scanDevices();
}

void BLEManager::loop() {
    if (!deviceConnected && reconnecting &&
        millis() - lastReconnectAttempt >= reconnectInterval) {
        logBLE("Attempting reconnect...");
        lastReconnectAttempt = millis();
        scanDevices();
    }
}

void BLEManager::heartRateNotifyCallback(BLERemoteCharacteristic* pChar,
                                         uint8_t* pData, size_t length, bool isNotify) {
    if (length > 1) {
        uint8_t hrValue = pData[1]; // Byte ke-1 = Heart Rate
        lastHeartRate = hrValue;
        logBLE("Heart Rate: " + String(hrValue));
    } else {
        logBLE("Invalid Heart Rate packet.");
    }
}
