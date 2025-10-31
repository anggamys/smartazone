#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include "data.h"

// Struktur data BLE mentah terakhir
struct BLEData
{
    uint8_t data;
    bool isNew;
    uint32_t timestamp;
};

class BLEManager
{
public:
    explicit BLEManager(const char *targetAddress, uint32_t scanTime = 6);
    void begin(const char *deviceName = "EoRa-S3-BLE");
    bool connect();
    bool tryReconnect();
    bool isConnected() const { return (deviceConnected && pClient && pClient->isConnected()); }

    bool enableNotify(const BLEUUID &serviceUUID, const BLEUUID &charUUID, void(*callback)(BLERemoteCharacteristic*, uint8_t*, size_t, bool));

    // Sensor trigger commands
    bool triggerSpO2();
    bool triggerStress();
    BLEData getLastSpO2();
    BLEData getLastStress();
    BLEData getLastHR();

    // Generic write function
    bool writeBytes(const BLEUUID &serviceUUID, const BLEUUID &charUUID, const uint8_t *data, size_t len);

    static DeviceData BLEDataToSensorData(uint8_t device_id, Topic topic, BLEData data);

private:
    const char *targetAddress;
    static BLEManager *instance;
    uint32_t scanTime;
    bool deviceConnected;
    unsigned long lastReconnectAttempt;
    static constexpr unsigned long reconnectInterval = 5000;
    BLEData HR, SpO2, Stress;

    BLEClient *pClient;

    BLEAddress scanTarget();
    BLERemoteCharacteristic *getCharacteristic(const BLEUUID &serviceUUID, const BLEUUID &charUUID);

    static void notifyThunk(BLERemoteCharacteristic *ch, uint8_t *data, size_t len, bool isNotify);
    static void HRNotifyCallback(BLERemoteCharacteristic *ch, uint8_t *data, size_t len, bool isNotify);

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
