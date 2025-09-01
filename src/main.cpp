#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

// Ganti sesuai MAC device BLE target (jam)
const char* targetAddress = "f8:fd:e8:84:37:89";
const int scanTime = 5; // durasi scan dalam detik

BLEClient* pClient;
bool deviceConnected = false;

// UUID placeholder, ganti sesuai service & characteristic jam BLE
BLEUUID serviceUUID("00001805-0000-1000-8000-00805f9b34fb"); // Current Time Service contoh
BLEUUID charUUID("00002a2b-0000-1000-8000-00805f9b34fb");    // Current Time Characteristic

// Callback client (connect/disconnect)
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pClient) {
    Serial.println("ESP32 berhasil connect ke device BLE!");
    deviceConnected = true;
  }

  void onDisconnect(BLEClient* pClient) {
    Serial.println("Terputus dari device BLE, mencoba reconnect...");
    deviceConnected = false;
  }
};

// Fungsi untuk connect ke device target
bool connectToServer(BLEAddress pAddress) {
  Serial.print("Mencoba connect ke: ");
  Serial.println(pAddress.toString().c_str());

  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if (pClient->connect(pAddress)) {
    Serial.println("Terhubung ke device BLE!");
    deviceConnected = true;
    return true;
  } else {
    Serial.println("Gagal connect.");
    deviceConnected = false;
    return false;
  }
}

// Scan dan connect
void scanAndConnect() {
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  Serial.println("Scanning BLE devices...");
  BLEScanResults results = pBLEScan->start(scanTime);

  bool found = false;
  for (int i = 0; i < results.getCount(); i++) {
    BLEAdvertisedDevice dev = results.getDevice(i);
    Serial.println(dev.toString().c_str());

    if (dev.getAddress().toString() == targetAddress) {
      Serial.println("Device target ditemukan!");
      found = true;
      connectToServer(dev.getAddress());
      break;
    }
  }

  if (!found) {
    Serial.println("Device target tidak ditemukan, coba scan ulang nanti...");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Inisialisasi BLE Client...");
  BLEDevice::init("");

  scanAndConnect();
}

void loop() {
  if (deviceConnected) {
    // Contoh membaca characteristic (Current Time Service)
    try {
      BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
      if (pRemoteService != nullptr) {
        BLERemoteCharacteristic* pRemoteChar = pRemoteService->getCharacteristic(charUUID);
        if (pRemoteChar != nullptr) {
          std::string value = pRemoteChar->readValue();
          Serial.print("Nilai characteristic: ");
          Serial.println(value.c_str());
        }
      }
    } catch (...) {
      Serial.println("Error baca characteristic");
    }
  } else {
    // Kalau disconnect, otomatis akan scan ulang
    scanAndConnect();
  }

  delay(3000); // loop setiap 3 detik
}
