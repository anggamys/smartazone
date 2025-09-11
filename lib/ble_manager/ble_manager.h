#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <functional>

class BLEManager {
public:
    explicit BLEManager(const char* targetAddress, uint32_t scanTime = 5);
    void begin();

    bool connectToServer(BLEAddress address);
    bool connectToService(const BLEUUID& serviceUUID);
    bool enableNotify(const BLEUUID& serviceUUID, const BLEUUID& charUUID);
    bool tryReconnect();

    bool isConnected() const { return deviceConnected; }
    int  getLastHeartRate() const { return lastHeartRate; }

    void setHRAvailableFlag(bool* flag, int* buffer);

    // event flag getter
    bool popLog(String& out);

private:
    const char* targetAddress;
    uint32_t scanTime;
    bool deviceConnected;
    bool reconnecting;
    unsigned long lastReconnectAttempt;
    static constexpr unsigned long reconnectInterval = 5000; // ms
    BLEClient* pClient;

    static int lastHeartRate;
    static void heartRateNotifyCallback(BLERemoteCharacteristic* pChar,
                                        uint8_t* pData, size_t length, bool isNotify);

    void scanDevices();

    // internal logging queue - optimized for memory
    static void pushLog(const char* msg);
    static char logBuffer[64];  // Fixed size buffer instead of String
    static bool   hasLog;

    class MyClientCallback : public BLEClientCallbacks {
    public:
        explicit MyClientCallback(BLEManager* parent) : parent(parent) {}
        void onConnect(BLEClient* pClient) override;
        void onDisconnect(BLEClient* pClient) override;
    private:
        BLEManager* parent;
    };
};
