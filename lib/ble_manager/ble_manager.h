#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// Struktur data mentah hasil BLE notify
struct BleData
{
    uint8_t payload[64];
    size_t length;
    uint32_t timestamp;
    bool ready;
    BLEUUID serviceUUID;
    BLEUUID charUUID;
    bool isHeartRate; // tambahkan flag agar SensorManager tahu ini data HR
};

class BLEManager
{
public:
    explicit BLEManager(const char *targetAddress, uint32_t scanTime = 6);
    void begin(const char *deviceName = "EoRa-S3-BLE");
    bool connect();
    bool tryReconnect();
    bool isConnected() const { return deviceConnected; }

    // Subscribe notify pada karakteristik (generic)
    bool enableNotify(const BLEUUID &serviceUUID, const BLEUUID &charUUID);

    // Menulis 1 byte ke characteristic (misal control point)
    bool writeByte(const BLEUUID &serviceUUID, const BLEUUID &charUUID, uint8_t value);

    // Ambil data BLE terakhir (raw)
    const BleData &getLastData() const { return lastBleData; }

    // Pop log event ringkas
    bool popLog(String &out);

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
