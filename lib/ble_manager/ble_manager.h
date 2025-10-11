#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

struct BleData
{
    uint8_t payload[64];
    size_t length;
    uint32_t timestamp;
    bool ready;
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

    // subscribe notify (isHR=true untuk 0x2A37)
    bool enableNotify(const BLEUUID &serviceUUID, const BLEUUID &charUUID, bool isHR = false);

    // tulis 1 byte ke control point (contoh 0x2A39)
    bool writeControl(const BLEUUID &serviceUUID, const BLEUUID &ctrlUUID, uint8_t value);

    // akses data terakhir
    const BleData &getLastData() const { return lastBleData; }

    // log ringkas
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

    static void pushLog(const char *msg);
    static void notifyThunk(BLERemoteCharacteristic *ch, uint8_t *data, size_t len, bool isNotify);

    static char logBuffer[128];
    static bool hasLog;
    static BleData lastBleData;

    class MyClientCallback : public BLEClientCallbacks
    {
    public:
        explicit MyClientCallback(BLEManager *p) : parent(p) {}
        void onConnect(BLEClient *) override;
        void onDisconnect(BLEClient *) override;

    private:
        BLEManager *parent;
    } clientCb;
};
