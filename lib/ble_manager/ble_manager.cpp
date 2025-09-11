#include "ble_manager.h"
#include <cstring>

// static members
char BLEManager::logBuffer[128] = {0};
bool BLEManager::hasLog = false;
int BLEManager::lastHeartRate = -1;

// HR pointer storage
static bool* hrFlagPtr = nullptr;
static int* hrBufferPtr = nullptr;

BLEManager::BLEManager(const char* targetAddress, uint32_t scanTime)
    : targetAddress(targetAddress),
      scanTime(scanTime),
      deviceConnected(false),
      lastReconnectAttempt(0),
      pClient(nullptr),
      pClientCb(nullptr) {}

void BLEManager::setHRAvailableFlag(bool* flag, int* buffer) {
    hrFlagPtr = flag;
    hrBufferPtr = buffer;
}

void BLEManager::pushLog(const char* msg) {
    // small safe copy
    strncpy(BLEManager::logBuffer, msg, sizeof(BLEManager::logBuffer) - 1);
    BLEManager::logBuffer[sizeof(BLEManager::logBuffer) - 1] = '\0';
    BLEManager::hasLog = true;
}

bool BLEManager::popLog(String& out) {
    if (hasLog) {
        out = String(BLEManager::logBuffer);
        hasLog = false;
        return true;
    }
    return false;
}

// callbacks
void BLEManager::MyClientCallback::onConnect(BLEClient*) {
    parent->deviceConnected = true;
    parent->lastReconnectAttempt = 0;
    BLEManager::pushLog("[BLE] Callback: connected");
}

void BLEManager::MyClientCallback::onDisconnect(BLEClient*) {
    parent->deviceConnected = false;
    BLEManager::pushLog("[BLE] Callback: disconnected");
}

bool BLEManager::connectToServer(BLEAddress address) {
    BLEManager::pushLog("[BLE] Connecting to server...");

    if (pClient != nullptr) {
        if (pClient->isConnected()) {
            BLEManager::pushLog("[BLE] Already connected.");
            return true;
        }
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        if (pClientCb) {
            delete pClientCb;
            pClientCb = nullptr;
        }
    }

    pClient = BLEDevice::createClient();
    pClientCb = new MyClientCallback(this);
    pClient->setClientCallbacks(pClientCb);

    bool ok = pClient->connect(address);
    if (ok) {
        BLEManager::pushLog("[BLE] Successfully connected to server.");
        deviceConnected = true;
        return true;
    } else {
        BLEManager::pushLog("[BLE] Failed to connect to server.");
        deviceConnected = false;
        return false;
    }
}

bool BLEManager::connectToService(const BLEUUID& serviceUUID) {
    if (!deviceConnected || pClient == nullptr) {
        BLEManager::pushLog("[BLE] connectToService: not connected.");
        return false;
    }
    BLERemoteService* svc = pClient->getService(serviceUUID);
    if (!svc) {
        BLEManager::pushLog("[BLE] Service not found.");
        return false;
    }
    BLEManager::pushLog("[BLE] Service found.");
    return true;
}

bool BLEManager::enableNotify(const BLEUUID& serviceUUID, const BLEUUID& charUUID) {
    if (!deviceConnected || pClient == nullptr) {
        BLEManager::pushLog("[BLE] enableNotify: not connected.");
        return false;
    }
    BLERemoteService* svc = pClient->getService(serviceUUID);
    if (!svc) {
        BLEManager::pushLog("[BLE] enableNotify: service not found.");
        return false;
    }
    BLERemoteCharacteristic* ch = svc->getCharacteristic(charUUID);
    if (!ch) {
        BLEManager::pushLog("[BLE] enableNotify: char not found.");
        return false;
    }
    if (!ch->canNotify()) {
        BLEManager::pushLog("[BLE] enableNotify: char cannot notify.");
        return false;
    }
    ch->registerForNotify(heartRateNotifyCallback);
    BLEManager::pushLog("[BLE] Notifications registered.");
    return true;
}

void BLEManager::scanDevices() {
    BLEManager::pushLog("[BLE] Starting scan...");
    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(50);

    BLEScanResults results = scan->start(scanTime, false);
    bool found = false;
    String target = String(targetAddress);
    target.toLowerCase();
    for (int i = 0; i < results.getCount(); ++i) {
        BLEAdvertisedDevice dev = results.getDevice(i);
        String addr = String(dev.getAddress().toString().c_str());
        addr.toLowerCase();
        if (addr == target) {
            char tmp[128];
            snprintf(tmp, sizeof(tmp), "[BLE] Found target: %s", addr.c_str());
            BLEManager::pushLog(tmp);
            found = true;
            connectToServer(dev.getAddress());
            break;
        }
    }
    if (!found) {
        BLEManager::pushLog("[BLE] Target not found");
    }
    scan->clearResults();
}

void BLEManager::begin(const char* deviceName) {
    BLEManager::pushLog("[BLE] BLE begin");
    BLEDevice::deinit(true);
    delay(150);
    BLEDevice::init(deviceName);
    delay(100);
    scanDevices();
}

bool BLEManager::tryReconnect() {
    unsigned long now = millis();
    if (!deviceConnected && (now - lastReconnectAttempt >= reconnectInterval)) {
        lastReconnectAttempt = now;
        BLEManager::pushLog("[BLE] tryReconnect: scanning...");
        scanDevices();
        return true;
    }
    return false;
}

// notify callback for HR characteristic
void BLEManager::heartRateNotifyCallback(BLERemoteCharacteristic*, uint8_t* pData, size_t length, bool) {
    if (length > 1) {
        bool is16 = pData[0] & 0x01;
        uint16_t hr = is16 ? (pData[1] | (pData[2] << 8)) : pData[1];
        lastHeartRate = hr;
        if (hrFlagPtr && hrBufferPtr) {
            *hrBufferPtr = (int)hr;
            *hrFlagPtr = true;
        }
        // we cannot call pushLog here (ISR-like), but set a small flag handled by main
    }
}
