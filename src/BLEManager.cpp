#include "BLEManager.h"

BLEManager::BLEManager(const char* targetAddress, int scanTime)
    : targetAddress(targetAddress),
      scanTime(scanTime),
      deviceConnected(false),
      reconnecting(false),
      lastReconnectAttempt(0),
      reconnectInterval(5000),
      hasShownServices(false),
      pClient(nullptr),
      currentServiceIndex(0),
      currentCharIndex(0)
{
}

void BLEManager::MyClientCallback::onConnect(BLEClient* pClient) {
    Serial.println("[BLE] Terhubung ke device BLE!");
}

void BLEManager::MyClientCallback::onDisconnect(BLEClient* pClient) {
    Serial.println("[BLE] Terputus dari device BLE, mencoba reconnect...");
    parent->deviceConnected = false;
    parent->scheduleReconnect();
}

bool BLEManager::connectToServer(BLEAddress pAddress) {
    Serial.print("[BLE] Mencoba connect ke: ");
    Serial.println(pAddress.toString().c_str());

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback(this));

    if (pClient->connect(pAddress)) {
        Serial.println("[BLE] Terhubung!");
        deviceConnected = true;
        reconnecting = false;
        reconnectInterval = 5000;
        hasShownServices = false; // reset flag
        currentServiceIndex = 0;
        currentCharIndex = 0;
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
        Serial.println("[BLE] Device target tidak ditemukan, scan ulang nanti...");
        scheduleReconnect();
    }
}

void BLEManager::showServiceValues() {
    if (!deviceConnected || !pClient) return;

    static std::map<std::string, std::vector<BLERemoteCharacteristic*>> serviceMap;

    if (!hasShownServices) {
        Serial.println("[BLE] Menampilkan semua service & characteristic (bertahap)...");

        auto services = pClient->getServices();
        if (!services) return;

        // Simpan semua characteristic dalam map
        for (auto &serviceEntry : *services) {
            BLERemoteService* service = serviceEntry.second;
            if (!service) continue;

            auto characteristics = service->getCharacteristics();
            if (!characteristics) continue;

            serviceMap[service->getUUID().toString()] = {};
            for (auto &charEntry : *characteristics) {
                BLERemoteCharacteristic* characteristic = charEntry.second;
                if (characteristic) {
                    serviceMap[service->getUUID().toString()].push_back(characteristic);
                }
            }
        }

        hasShownServices = true;
        currentServiceIndex = 0;
        currentCharIndex = 0;
    }

    // Iterasi satu characteristic per loop
    auto it = serviceMap.begin();
    std::advance(it, currentServiceIndex);
    if (it != serviceMap.end()) {
        const std::string &serviceUUID = it->first;
        auto &charList = it->second;

        if (currentCharIndex < charList.size()) {
            BLERemoteCharacteristic* characteristic = charList[currentCharIndex];
            if (characteristic) {
                std::string value;
                try {
                    value = characteristic->readValue();
                } catch (...) {
                    value = "<read error>";
                }

                Serial.printf("[BLE] Service UUID: %s, Characteristic UUID: %s, Value: %s\n",
                              serviceUUID.c_str(),
                              characteristic->getUUID().toString().c_str(),
                              value.c_str());
            }
            currentCharIndex++;
        } else {
            // Lanjut ke service berikutnya
            currentServiceIndex++;
            currentCharIndex = 0;
        }
    }
}

void BLEManager::scheduleReconnect() {
    if (!reconnecting) {
        reconnecting = true;
        lastReconnectAttempt = millis();
        reconnectInterval = min(reconnectInterval * 2, 60000UL);
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
        showServiceValues(); // membaca satu characteristic per loop
        delay(50); // beri ESP32 waktu untuk BLE stack
    } else if (reconnecting && millis() - lastReconnectAttempt >= reconnectInterval) {
        Serial.println("[BLE] Mencoba reconnect...");
        reconnecting = false;
        scanAndConnect();
    }
}
