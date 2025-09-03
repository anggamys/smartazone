#include "BLEManager.h"

BLEManager::BLEManager(const char* targetAddress, int scanTime)
    : targetAddress(targetAddress),
      scanTime(scanTime),
      deviceConnected(false),
      reconnecting(false),
      lastReconnectAttempt(0),
      reconnectInterval(5000), // mulai 5 detik
      pClient(nullptr),
      serviceUUID("00001805-0000-1000-8000-00805f9b34fb"), // contoh: Current Time Service
      charUUID("00002a2b-0000-1000-8000-00805f9b34fb")     // Current Time characteristic
{}

void BLEManager::MyClientCallback::onConnect(BLEClient* pClient) {
    Serial.println("[BLE] ESP32 berhasil connect ke device BLE!");
}

void BLEManager::MyClientCallback::onDisconnect(BLEClient* pClient) {
    Serial.println("[BLE] Terputus dari device BLE, akan mencoba reconnect...");
    parent->deviceConnected = false;
    parent->scheduleReconnect();
}

bool BLEManager::connectToServer(BLEAddress pAddress) {
    Serial.print("[BLE] Mencoba connect ke: ");
    Serial.println(pAddress.toString().c_str());

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback(this));

    if (pClient->connect(pAddress)) {
        Serial.println("[BLE] Terhubung ke device BLE!");
        deviceConnected = true;
        reconnecting = false;
        reconnectInterval = 5000; // reset interval
        return true;
    } else {
        Serial.println("[BLE] Gagal connect.");
        deviceConnected = false;
        scheduleReconnect();
        return false;
    }
}

void BLEManager::scanAndConnect() {
    Serial.println("[BLE] Scanning BLE devices...");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    BLEScanResults results = pBLEScan->start(scanTime);

    bool found = false;
    for (int i = 0; i < results.getCount(); i++) {
        BLEAdvertisedDevice dev = results.getDevice(i);
        Serial.print("[BLE] Ditemukan: ");
        Serial.println(dev.toString().c_str());

        if (dev.getAddress().toString() == targetAddress) {
            Serial.println("[BLE] Device target ditemukan!");
            found = true;
            connectToServer(dev.getAddress());
            break;
        }
    }

    if (!found) {
        Serial.println("[BLE] Device target tidak ditemukan, akan coba ulang...");
        scheduleReconnect();
    }
}

void BLEManager::readCharacteristic() {
    if (!deviceConnected) return;

    try {
        BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
        if (pRemoteService != nullptr) {
            BLERemoteCharacteristic* pRemoteChar = pRemoteService->getCharacteristic(charUUID);
            if (pRemoteChar != nullptr) {
                std::string value = pRemoteChar->readValue();
                Serial.print("[BLE] Nilai characteristic: ");
                Serial.println(value.c_str());
            }
        }
    } catch (...) {
        Serial.println("[BLE] Error baca characteristic");
        deviceConnected = false;
        scheduleReconnect();
    }
}

void BLEManager::scheduleReconnect() {
    if (!reconnecting) {
        reconnecting = true;
        lastReconnectAttempt = millis();
        reconnectInterval = min(reconnectInterval * 2, (unsigned long)60000); // max 60 detik
        Serial.printf("[BLE] Reconnect dijadwalkan dalam %lu ms\n", reconnectInterval);
    }
}

void BLEManager::begin() {
    Serial.println("[BLE] Inisialisasi BLE Client...");
    BLEDevice::init("");
    scanAndConnect();
}

void BLEManager::loop() {
    if (deviceConnected) {
        readCharacteristic();
    } else if (reconnecting && millis() - lastReconnectAttempt >= reconnectInterval) {
        Serial.println("[BLE] Mencoba reconnect...");
        reconnecting = false;
        scanAndConnect();
    }
    delay(3000);
}
