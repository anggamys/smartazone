#include <Arduino.h>
#include "ble_manager.h"

// MAC address target
const char* targetAddress = "f8:fd:e8:84:37:89";

// Inisialisasi manager
BLEManager ble(targetAddress);

void setup() {
    Serial.begin(115200);
    ble.begin();
}

void loop() {
    ble.loop();
    delay(100); // beri waktu ke BLE stack
}
