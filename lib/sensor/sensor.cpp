#include "sensor.h"

SensorManager::SensorManager()
{
    currentData = {"NONE", 0.0f, 0, false};
}

void SensorManager::updateFromBle(const BleData &bleData)
{
    if (!bleData.ready)
    {
        currentData.valid = false;
        return;
    }

    const uint8_t *data = bleData.payload;
    size_t len = bleData.length;

    // ================= Heart Rate =================
    if (bleData.isHeartRate)
    {
        int hr = parseHeartRate(data, len);
        currentData = (hr > 0)
                          ? SensorData{"HR", (float)hr, bleData.timestamp, true}
                          : SensorData{"HR_RAW", -1, bleData.timestamp, false};
        return;
    }

    // ================= Aolon Curve Payload =================
    if (len >= 6 && data[0] == 0xFE && data[1] == 0xEA && data[2] == 0x20)
    {
        // Tipe data ditentukan oleh byte ke-3 (index 3)
        uint8_t typeByte = data[3];

        if (typeByte == 0x06) // SPO2
        {
            int spo2 = parseSpO2(data, len);
            currentData = (spo2 > 0)
                              ? SensorData{"SPO2", (float)spo2, bleData.timestamp, true}
                              : SensorData{"SPO2_RAW", -1, bleData.timestamp, false};
        }
        else if (typeByte == 0x08) // STRESS
        {
            int stress = parseStress(data, len);
            currentData = (stress >= 0)
                              ? SensorData{"STRESS", (float)stress, bleData.timestamp, true}
                              : SensorData{"STRESS_RAW", -1, bleData.timestamp, false};
        }
        else
        {
            currentData = {"RAW", 0, bleData.timestamp, false};
        }
    }
    else
    {
        currentData = {"RAW", 0, bleData.timestamp, false};
    }
}

// ================= Parsing =================

int SensorManager::parseHeartRate(const uint8_t *data, size_t len)
{
    if (len < 2)
        return -1;

    bool is16bit = data[0] & 0x01;
    int hr = is16bit && len >= 3 ? (data[1] | (data[2] << 8)) : data[1];
    return (hr >= 30 && hr <= 220) ? hr : -1;
}

int SensorManager::parseSpO2(const uint8_t *data, size_t len)
{
    // FE EA 20 06 6B 62 → ambil byte terakhir
    if (len < 6)
        return -1;

    uint8_t val = data[len - 1];
    return (val != 0xFF && val <= 100) ? val : -1;
}

int SensorManager::parseStress(const uint8_t *data, size_t len)
{
    // FE EA 20 08 B9 11 00 2C → ambil 2 byte terakhir (16 bit)
    if (len < 8)
        return -1;

    uint8_t low = data[len - 1];
    uint8_t high = data[len - 2];

    if (low == 0xFF && high == 0xFF)
        return -1;

    uint16_t val = (high << 8) | low;
    return val;
}
