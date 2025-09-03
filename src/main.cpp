#include <Arduino.h>
#include "BLEManager.h"

const char* targetAddress = "f8:fd:e8:84:37:89";
const int scanTime = 5;

BLEManager bleManager(targetAddress, scanTime);

void setup() {
    Serial.begin(115200);
    bleManager.begin();
}

void loop() {
    bleManager.loop();
}
