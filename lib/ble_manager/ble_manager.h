#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

class BLEManager {
public:
    explicit BLEManager(const char* targetAddress, uint32_t scanTime = 6);
    void begin(const char* deviceName = "EoRa-S3-BLE");

    bool connectToServer(BLEAddress address);
    bool connectToService(const BLEUUID& serviceUUID);
    bool enableNotify(const BLEUUID& serviceUUID, const BLEUUID& charUUID);
    bool tryReconnect();

    bool isConnected() const { return deviceConnected; }
    int  getLastHeartRate() const { return lastHeartRate; }

    // link to application HR buffers
    void setHRAvailableFlag(bool* flag, int* buffer);

    // pop logging messages produced by BLE manager
    bool popLog(String& out);

private:
    const char* targetAddress;
    uint32_t scanTime;
    bool deviceConnected;
    unsigned long lastReconnectAttempt;
    static constexpr unsigned long reconnectInterval = 5000; // ms
    BLEClient* pClient;
    BLEClientCallbacks* pClientCb;

    static int lastHeartRate;
    static void heartRateNotifyCallback(BLERemoteCharacteristic* pChar,
                                        uint8_t* pData, size_t length, bool isNotify);

    void scanDevices();

    // internal log storage (small ring of one message to avoid dynamic Strings)
    static void pushLog(const char* msg);
    static char logBuffer[128];
    static bool hasLog;

    class MyClientCallback : public BLEClientCallbacks {
    public:
        explicit MyClientCallback(BLEManager* parent) : parent(parent) {}
        void onConnect(BLEClient* pClient) override;
        void onDisconnect(BLEClient* pClient) override;
    private:
        BLEManager* parent;
    };
};
