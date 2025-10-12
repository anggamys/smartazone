#pragma once
#include <Arduino.h>
#include "ble_manager.h"

struct MultiSensorData
{
    float hr;
    float spo2;
    float stress;
    bool hr_valid;
    bool spo2_valid;
    bool stress_valid;
    uint32_t timestamp;
};

class SensorManager
{
public:
    SensorManager();
    void updateFromBle(const BleData &bleData);
    MultiSensorData getCombinedData() const;

private:
    MultiSensorData data;

    int parseHeartRate(const uint8_t *data, size_t len);
    int parseSpO2(const uint8_t *data, size_t len);
    int parseStress(const uint8_t *data, size_t len);
};
