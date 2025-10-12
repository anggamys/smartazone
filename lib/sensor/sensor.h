#pragma once
#include <Arduino.h>
#include "ble_manager.h"

struct SensorData
{
    String type; // HR, SPO2, STRESS
    float value; // nilai terukur
    uint32_t timestamp;
    bool valid;
};

class SensorManager
{
public:
    SensorManager();
    void updateFromBle(const BleData &bleData);
    const SensorData &getData() const { return currentData; }

private:
    SensorData currentData;

    int parseHeartRate(const uint8_t *data, size_t len);
    int parseSpO2(const uint8_t *data, size_t len);
    int parseStress(const uint8_t *data, size_t len);
};
