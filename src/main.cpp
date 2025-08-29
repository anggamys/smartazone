#include <Arduino.h>
#include "display_manager.h"
#include "ble_manager.h"

#define OLED_SDA 18
#define OLED_SCL 17
#define SCAN_TIME 5

DisplayManager oled(OLED_SDA, OLED_SCL);
BleManager ble(SCAN_TIME);

void setup() {
  Serial.begin(115200);

  if (!oled.begin()) {
    Serial.println("Display init failed!");
    while (true);
  }

  ble.begin();
  oled.printText("BLE Scanner Ready", 0, 10, 1);
  delay(2000);
}

void loop() {
  // Scan perangkat BLE
  ble.scanDevices();

  // Ambil semua nama device
  std::vector<String> devices = ble.getDeviceNames();

  if (devices.empty()) {
    oled.printTwoLine("No Device Found", "");
    delay(2000);
  } else {
    for (String name : devices) {
      oled.printTwoLine("Device:", name);
      delay(2000); // tampilkan tiap device 2 detik
    }
  }

  delay(1000); // jeda sebelum scan berikutnya
}
