#include "ble_manager.h"

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  private:
    std::vector<String>* devices;

  public:
    MyAdvertisedDeviceCallbacks(std::vector<String>* devs) {
      devices = devs;
    }

    void onResult(BLEAdvertisedDevice advertisedDevice) {
      String name;
      if (advertisedDevice.haveName()) {
        name = advertisedDevice.getName().c_str();
      } else {
        name = "Unknown";
      }

      devices->push_back(name);
      Serial.printf("Found: %s\n", name.c_str());
    }
};

BleManager::BleManager(int scanDuration) {
  scanTime = scanDuration;
  bleScan = nullptr;
}

void BleManager::begin() {
  BLEDevice::init("");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(&deviceNames));
  bleScan->setActiveScan(true);
  bleScan->setInterval(100);
  bleScan->setWindow(99);
}

void BleManager::scanDevices() {
  deviceNames.clear(); // reset sebelum scan baru
  Serial.println("Scanning...");
  BLEScanResults foundDevices = bleScan->start(scanTime, false);
  Serial.printf("Devices found: %d\n", foundDevices.getCount());
  bleScan->clearResults(); // free memory
}

std::vector<String> BleManager::getDeviceNames() {
  return deviceNames;
}
