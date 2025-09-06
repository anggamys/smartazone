#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

class BLEManager {
public:
    explicit BLEManager(const char* targetAddress);
    void begin();
    void loop();

    bool connectToServer(BLEAddress address);
    bool connectToService(const BLEUUID& serviceUUID);
    bool enableNotify(const BLEUUID& serviceUUID, const BLEUUID& charUUID);

    bool isConnected() const { return deviceConnected; }
    int getLastHeartRate() const { return lastHeartRate; }

private:
    const char* targetAddress;
    bool deviceConnected;
    bool reconnecting;
    unsigned long lastReconnectAttempt;
    static constexpr unsigned long reconnectInterval = 5000; // ms
    BLEClient* pClient;

    static int lastHeartRate;  // Heart Rate terakhir dari callback
    void scanDevices();

    class MyClientCallback : public BLEClientCallbacks {
    public:
        explicit MyClientCallback(BLEManager* parent) : parent(parent) {}
        void onConnect(BLEClient* pClient) override;
        void onDisconnect(BLEClient* pClient) override;
    private:
        BLEManager* parent;
    };

    // Callback untuk notify
    static void heartRateNotifyCallback(BLERemoteCharacteristic* pChar,
                                        uint8_t* pData, size_t length, bool isNotify);
};
