#include "sensor.h"

SensorManager::SensorManager()
{
    data = {0, 0, 0, false, false, false, 0};
}

void SensorManager::updateFromBle(const BleData &bleData)
{
    if (!bleData.ready)
        return;

    const uint8_t *d = bleData.payload;
    size_t len = bleData.length;

    Serial.print("[BLE RAW] ");
    for (size_t i = 0; i < len; i++)
    {
        Serial.printf("%02X ", d[i]);
    }
    Serial.println();

    data.timestamp = bleData.timestamp;

    // 1. Heart Rate standar (UUID 2A37)
    if (bleData.isHeartRate)
    {
        int hr = parseHeartRate(d, len);
        if (hr > 0)
        {
            data.hr = hr;
            data.hr_valid = true;
            Serial.printf("[BLE] HR (UUID) %d bpm\n", hr);
        }
        return;
    }

    // 2. Aolon custom packet
    if (len >= 5 && d[0] == 0xFE && d[1] == 0xEA && d[2] == 0x20)
    {
        uint8_t typeByte = d[3];

        if (typeByte == 0x06)
        {
            // Paket 0x06 bisa HR atau SPO2 tergantung panjang payload
            if (len <= 6)
            {
                // HR (contoh FE EA 20 06 5A)
                int hr = d[len - 1];
                if (hr >= 30 && hr <= 220)
                {
                    data.hr = hr;
                    data.hr_valid = true;
                    Serial.printf("[BLE] HR (FEEA) %d bpm\n", hr);
                }
            }
            else
            {
                // SPO2 (contoh FE EA 20 06 6B 62)
                int spo2 = parseSpO2(d, len);
                if (spo2 > 0)
                {
                    data.spo2 = spo2;
                    data.spo2_valid = true;
                    Serial.printf("[BLE] SPO2 %d %%\n", spo2);
                }
            }
        }
        else if (typeByte == 0x08)
        {
            int stress = parseStress(d, len);
            if (stress >= 0)
            {
                data.stress = stress;
                data.stress_valid = true;
                Serial.printf("[BLE] STRESS %d\n", stress);
            }
        }
        else
        {
            Serial.println("[BLE] Unknown FEEA type");
        }
    }
}

MultiSensorData SensorManager::getCombinedData() const
{
    return data;
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
    if (len < 6)
        return -1;
    uint8_t val = data[len - 1];
    return (val != 0xFF && val <= 100) ? val : -1;
}

int SensorManager::parseStress(const uint8_t *data, size_t len)
{
    if (len < 8)
        return -1;
    uint8_t low = data[len - 1];
    uint8_t high = data[len - 2];
    if (low == 0xFF && high == 0xFF)
        return -1;
    uint16_t val = (high << 8) | low;
    return val;
}
