#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

class BLEManager {
public:
    BLEManager(const char* targetAddress, uint32_t scanTime = 5);
    void begin();
    void loop();

private:
    const char* targetAddress;
    uint32_t scanTime;
    bool deviceConnected;
    bool reconnecting;
    uint32_t lastReconnectAttempt;
    uint32_t reconnectInterval;
    BLEClient* pClient;

    void scanDevices();
    bool connectToServer(const BLEAddress& macAddress);
    void scheduleReconnect();

    class MyClientCallback : public BLEClientCallbacks {
    public:
        explicit MyClientCallback(BLEManager* parent) : parent(parent) {}
        void onConnect(BLEClient* client) override;
        void onDisconnect(BLEClient* client) override;
    private:
        BLEManager* parent;
    };
};
