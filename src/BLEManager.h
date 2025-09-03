// BLEManager.h
#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <map>
#include <vector>

class BLEManager {
public:
    BLEManager(const char* targetAddress, int scanTime = 5);
    void begin();
    void loop();

private:
    const char* targetAddress;
    int scanTime;
    bool deviceConnected;
    bool reconnecting;
    unsigned long lastReconnectAttempt;
    unsigned long reconnectInterval;
    bool hasShownServices;
    BLEClient* pClient;

    // ======= Tambahkan ini =======
    size_t currentServiceIndex;
    size_t currentCharIndex;
    // ============================

    void scanAndConnect();
    bool connectToServer(BLEAddress pAddress);
    void showServiceValues();
    void scheduleReconnect();

    class MyClientCallback : public BLEClientCallbacks {
    public:
        MyClientCallback(BLEManager* parent) : parent(parent) {}
        void onConnect(BLEClient* pClient);
        void onDisconnect(BLEClient* pClient);
    private:
        BLEManager* parent;
    };
};
