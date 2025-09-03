#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

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

    BLEClient* pClient;
    BLEUUID serviceUUID;
    BLEUUID charUUID;

    class MyClientCallback : public BLEClientCallbacks {
        BLEManager* parent;
    public:
        MyClientCallback(BLEManager* p) : parent(p) {}
        void onConnect(BLEClient* pClient) override;
        void onDisconnect(BLEClient* pClient) override;
    };

    bool connectToServer(BLEAddress pAddress);
    void scanAndConnect();
    void readCharacteristic();
    void scheduleReconnect();
};

#endif
