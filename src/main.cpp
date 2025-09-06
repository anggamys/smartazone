#include <Arduino.h>
#include "ble_manager.h"

const char* targetAddress = "f8:fd:e8:84:37:89";

// UUID Heart Rate Service & Characteristic (standard BLE spec)
static BLEUUID heartRateService("0000180d-0000-1000-8000-00805f9b34fb");
static BLEUUID heartRateChar("00002a37-0000-1000-8000-00805f9b34fb");

BLEManager ble(targetAddress);

void setup() {
    Serial.begin(115200);
    ble.begin();
}

void loop() {
    ble.loop();

    static bool serviceConnected = false;
    static bool notifyEnabled = false;

    if (ble.isConnected()) {
        if (!serviceConnected) {
            serviceConnected = ble.connectToService(heartRateService);
        }
        if (serviceConnected && !notifyEnabled) {
            notifyEnabled = ble.enableNotify(heartRateService, heartRateChar);
        }
    }

    delay(500);
}
