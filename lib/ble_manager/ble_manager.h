#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

class BLEManager
{
public:
    explicit BLEManager(const char *targetAddress, uint32_t scanTime = 6);
    void begin(const char *deviceName = "EoRa-S3-BLE");

    bool connect();
    bool connectToServiceAndNotify(const BLEUUID &serviceUUID, const BLEUUID &charUUID);
    bool tryReconnect();

    bool isConnected() const { return deviceConnected; }
    int getLastHeartRate() const { return lastHeartRate; }

    void setHRAvailableFlag(bool *flag, int *buffer);
    bool popLog(String &out);

private:
    const char *targetAddress;
    uint32_t scanTime;
    bool deviceConnected;
    unsigned long lastReconnectAttempt;
    static constexpr unsigned long reconnectInterval = 5000;

    BLEClient *pClient;
    class MyClientCallback;

    static int lastHeartRate;
    static void heartRateNotifyCallback(BLERemoteCharacteristic *pChar,
                                        uint8_t *pData, size_t length, bool isNotify);

    BLEAddress scanTarget();

    static void pushLog(const char *msg);
    static char logBuffer[128];
    static bool hasLog;

    class MyClientCallback : public BLEClientCallbacks
    {
    public:
        explicit MyClientCallback(BLEManager *parent) : parent(parent) {}
        void onConnect(BLEClient *) override;
        void onDisconnect(BLEClient *) override;

    private:
        BLEManager *parent;
    } clientCb;
};
