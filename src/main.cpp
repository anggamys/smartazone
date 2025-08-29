#include <Arduino.h>
#include "display_manager.h"

#define OLED_SDA 18
#define OLED_SCL 17

DisplayManager oled(OLED_SDA, OLED_SCL);

void setup() {
  Serial.begin(115200);

  if (!oled.begin()) {
    Serial.println("Display init failed!");
    while (true);
  }

  oled.printText("Hello ESP32-S2!", 0, 10, 2);
  delay(2000);

  oled.printTwoLine("PlatformIO", "OLED Manager");
  delay(2000);

  for (int i = 0; i <= 100; i += 20) {
    oled.clear();
    oled.printText("Loading...", 0, 0, 1, false);
    oled.drawProgressBar(10, 30, 100, 10, i);
    delay(500);
  }
}

void loop() {
  oled.printText("Loop Running", 0, 20, 1);
  delay(2000);
}
