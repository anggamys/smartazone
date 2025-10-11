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

    if (bleData.isHeartRate)
    {
        int hr = parseHeartRate(bleData.payload, bleData.length);
        if (hr > 0)
        {
            currentData.type = "HR";
            currentData.value = hr;
            currentData.timestamp = bleData.timestamp;
            currentData.valid = true;
        }
        else
        {
            currentData.type = "HR_RAW";
            currentData.value = -1;
            currentData.timestamp = bleData.timestamp;
            currentData.valid = false;
        }
    }
    else
    {
        // fallback untuk data sensor lain
        currentData.type = "RAW";
        currentData.value = 0;
        currentData.timestamp = bleData.timestamp;
        currentData.valid = false;
    }
}

int SensorManager::parseHeartRate(const uint8_t *data, size_t len)
{
    if (len < 2)
        return -1;

    bool is16bit = data[0] & 0x01;
    int hr = is16bit && len >= 3 ? (data[1] | (data[2] << 8)) : data[1];
    if (hr < 30 || hr > 220)
        return -1;
    return hr;
}

String SensorManager::toHexString(const uint8_t *p, size_t n)
{
    String s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; i++)
    {
        if (p[i] < 16)
            s += "0";
        s += String(p[i], HEX);
    }
    s.toUpperCase();
    return s;
}
