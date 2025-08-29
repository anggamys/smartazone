#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <vector>

class BleManager {
  private:
    BLEScan* bleScan;
    int scanTime;
    std::vector<String> deviceNames;

  public:
    BleManager(int scanDuration = 5);
    void begin();
    void scanDevices();
    std::vector<String> getDeviceNames();  // ambil semua nama device
};

#endif
