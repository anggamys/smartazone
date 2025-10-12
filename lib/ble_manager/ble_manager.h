#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// Struktur data BLE mentah terakhir
struct BleData
{
    uint8_t payload[64];
    size_t length;
    uint32_t timestamp;
    bool ready;
    BLEUUID serviceUUID;
    BLEUUID charUUID;
    bool isHeartRate;
};

class BLEManager
{
public:
    explicit BLEManager(const char *targetAddress, uint32_t scanTime = 6);
    void begin(const char *deviceName = "EoRa-S3-BLE");
    bool connect();
    bool tryReconnect();
    bool isConnected() const { return deviceConnected; }

    bool enableNotify(const BLEUUID &serviceUUID, const BLEUUID &charUUID);

    const BleData &getLastData() const { return lastBleData; }
    bool popLog(String &out);

    // Sensor trigger commands
    bool triggerSpO2();
    bool triggerStress();

    // Generic write function
    bool writeBytes(const BLEUUID &serviceUUID, const BLEUUID &charUUID, const uint8_t *data, size_t len);

private:
    const char *targetAddress;
    uint32_t scanTime;
    bool deviceConnected;
    unsigned long lastReconnectAttempt;
    static constexpr unsigned long reconnectInterval = 5000;

    BLEClient *pClient;

    BLEAddress scanTarget();
    BLERemoteCharacteristic *getCharacteristic(const BLEUUID &serviceUUID, const BLEUUID &charUUID);

    static void notifyThunk(BLERemoteCharacteristic *ch, uint8_t *data, size_t len, bool isNotify);
    static void pushLog(const char *msg);

    static char logBuffer[128];
    static bool hasLog;
    static BleData lastBleData;

    class MyClientCallback : public BLEClientCallbacks
    {
    public:
        explicit MyClientCallback(BLEManager *parent) : parent_(parent) {}
        void onConnect(BLEClient *) override;
        void onDisconnect(BLEClient *) override;

    private:
        BLEManager *parent_;
    } clientCb;
};
