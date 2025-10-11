#pragma once
#include <Arduino.h>
#include "ble_manager.h"

struct SensorData
{
    String type; // contoh: "HR"
    float value; // hasil parsing
    uint32_t timestamp;
    bool valid;
};

class SensorManager
{
public:
    SensorManager();
    void updateFromBle(const BleData &bleData);
    const SensorData &getData() const { return currentData; }

    // Boleh public agar bisa digunakan untuk debug
    String toHexString(const uint8_t *p, size_t n);

private:
    SensorData currentData;

    int parseHeartRate(const uint8_t *data, size_t len);
};
