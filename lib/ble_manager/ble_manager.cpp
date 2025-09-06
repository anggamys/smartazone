#include "ble_manager.h"

BLEManager::BLEManager(const char* targetAddress, uint32_t scanTime)
    : targetAddress(targetAddress),
      scanTime(scanTime),
      deviceConnected(false),
      reconnecting(false),
      lastReconnectAttempt(0),
      reconnectInterval(5000),
      pClient(nullptr) {}

void BLEManager::MyClientCallback::onConnect(BLEClient* client) {
    Serial.println("[BLE] Terhubung ke device BLE!");
    parent->deviceConnected = true;
    parent->reconnecting = false;
}

void BLEManager::MyClientCallback::onDisconnect(BLEClient* client) {
    Serial.println("[BLE] Terputus dari device BLE, mencoba reconnect...");
    parent->deviceConnected = false;
    parent->scheduleReconnect();
}

bool BLEManager::connectToServer(const BLEAddress& macAddress) {
    Serial.print("[BLE] Mencoba connect ke: ");
    {
        BLEAddress addr = macAddress;
        Serial.println(addr.toString().c_str());
    }

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback(this));

    if (pClient->connect(macAddress)) {
        Serial.println("[BLE] Terhubung ke server BLE!");
        deviceConnected = true;
        reconnecting = false;
        return true;
    }

    Serial.println("[BLE] Gagal connect.");
    scheduleReconnect();
    return false;
}

void BLEManager::scanDevices() {
    Serial.println("[BLE] Scanning BLE devices...");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    BLEScanResults results = pBLEScan->start(scanTime);

    for (int i = 0; i < results.getCount(); i++) {
        BLEAdvertisedDevice dev = results.getDevice(i);
        Serial.printf("[BLE] Ditemukan: %s\n", dev.getAddress().toString().c_str());

        if (dev.getAddress().toString() == targetAddress) {
            Serial.println("[BLE] Device target ditemukan!");
            connectToServer(dev.getAddress());
            return;
        }
    }

    Serial.println("[BLE] Device target tidak ditemukan, scan ulang nanti...");
    scheduleReconnect();
}

void BLEManager::scheduleReconnect() {
    reconnecting = true;
    lastReconnectAttempt = millis();
    uint32_t newInterval = reconnectInterval * 2U;
    reconnectInterval = (newInterval < (uint32_t)60000U) ? newInterval : (uint32_t)60000U;
    Serial.printf("[BLE] Reconnect dijadwalkan dalam %lu ms\n", reconnectInterval);
}

void BLEManager::begin() {
    Serial.println("[BLE] Inisialisasi BLE Client...");
    BLEDevice::init("");
    scanDevices();
}

void BLEManager::loop() {
    if (!deviceConnected && reconnecting) {
        if (millis() - lastReconnectAttempt >= reconnectInterval) {
            Serial.println("[BLE] Mencoba reconnect...");
            reconnecting = false;
            scanDevices();
        }
    }
}
