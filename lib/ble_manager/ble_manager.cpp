#include "ble_manager.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// Mutex untuk mencegah race condition antar thread BLE
static SemaphoreHandle_t bleMutex = xSemaphoreCreateMutex();
BLEManager *BLEManager::instance = nullptr;

// UUID service & characteristic Aolon Curve
static const BLEUUID GENERIC_SERVICE("0000feea-0000-1000-8000-00805f9b34fb");
static const BLEUUID CHAR_WRITE("0000fee2-0000-1000-8000-00805f9b34fb");
static const BLEUUID CHAR_NOTIFY("0000fee3-0000-1000-8000-00805f9b34fb");
static const BLEUUID HR_SERVICE("0000180d-0000-1000-8000-00805f9b34fb");
static const BLEUUID HR_NOTIFY_CHAR("00002a37-0000-1000-8000-00805f9b34fb");

BLEManager::BLEManager(const char *targetAddress, uint32_t scanTime)
    : targetAddress(targetAddress),
      scanTime(scanTime),
      deviceConnected(false),
      lastReconnectAttempt(0),
      pClient(nullptr),
      clientCb(this)
{
    instance = this;
}

// ===========================================
// Callback koneksi
// ===========================================
void BLEManager::MyClientCallback::onConnect(BLEClient *)
{
    parent_->deviceConnected = true;
    parent_->lastReconnectAttempt = 0;
    Serial.println("[BLE] Connected");
}

void BLEManager::MyClientCallback::onDisconnect(BLEClient *)
{
    parent_->deviceConnected = false;
    Serial.println("[BLE] Disconnected");
    delay(300); // beri waktu cleanup stack BLE internal
}

// ===========================================
// Scan dan koneksi
// ===========================================
BLEAddress BLEManager::scanTarget()
{
    BLEScan *scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    Serial.println("[BLE] Scanning for target...");
    BLEScanResults results = scan->start(scanTime, false);
    Serial.printf("[BLE] Found %d devices\n", results.getCount());

    String target = String(targetAddress);
    target.toLowerCase();

    for (int i = 0; i < results.getCount(); ++i)
    {
        BLEAdvertisedDevice dev = results.getDevice(i);
        String addr = String(dev.getAddress().toString().c_str());
        addr.toLowerCase();
        if (addr == target)
        {
            Serial.printf("[BLE] Found target %s\n", dev.getAddress().toString().c_str());
            return dev.getAddress();
        }
    }
    Serial.println("[BLE] Device not found");
    return BLEAddress("");
}

void BLEManager::begin(const char *deviceName)
{
    BLEDevice::deinit(true);
    delay(200);
    BLEDevice::init(deviceName);
    delay(200);
    connect();
}

bool BLEManager::connect()
{
    BLEAddress addr = scanTarget();
    if (!addr.toString().length())
    {
        Serial.println("[BLE] Device not found during scan");
        return false;
    }

    if (!pClient)
        pClient = BLEDevice::createClient();
    if (pClient->isConnected())
    {
        Serial.println("[BLE] Device has already connected");
        return true;
    }

    pClient->setClientCallbacks(&clientCb);

    Serial.printf("[BLE] Connecting to %s...\n", addr.toString().c_str());
    if (!pClient->connect(addr))
    {
        Serial.println("[BLE] Connect failed");
        return false;
    }

    deviceConnected = true;
    Serial.println("[BLE] Connected to server");

// Tampilkan daftar service untuk debug
#ifdef DEBUG
    std::map<std::string, BLERemoteService *> *services = pClient->getServices();
    if (services && !services->empty())
    {
        Serial.println("[BLE] Services discovered:");
        for (auto &s : *services)
            Serial.println("  " + String(s.first.c_str()));
    }
    else
    {
        Serial.println("[BLE] No services discovered (may still work)");
    }
#endif

    delay(500);
    enableNotify(GENERIC_SERVICE, CHAR_NOTIFY,&BLEManager::notifyThunk);
    enableNotify(HR_SERVICE, HR_NOTIFY_CHAR,&BLEManager::HRNotifyCallback);
    return true;
}

// ===========================================
// Reconnect handler
// ===========================================
bool BLEManager::tryReconnect()
{
    unsigned long now = millis();
    if (!deviceConnected && (now - lastReconnectAttempt >= reconnectInterval))
    {
        lastReconnectAttempt = now;
        return connect();
    }
    return false;
}

// ===========================================
// Akses ke karakteristik
// ===========================================
BLERemoteCharacteristic *BLEManager::getCharacteristic(const BLEUUID &serviceUUID, const BLEUUID &charUUID)
{
    if (!isConnected())
    {
        Serial.println("[BLE] device is not connected, cannot get charateristic");
        return nullptr;
    }

    BLERemoteService *svc = nullptr;
    // looping max 3 times to get service
    for (int i = 0; i < 3 && !svc; i++)
    {
        svc = pClient->getService(serviceUUID);
        if (!svc)
            delay(200);
    }

    if (!svc)
    {
        Serial.println("[BLE] Service not found");
        return nullptr;
    }

    BLERemoteCharacteristic *ch = svc->getCharacteristic(charUUID);
    if (!ch)
    {
        Serial.println("[BLE] Char not found");
        return nullptr;
    }

    return ch;
}

// ===========================================
// Enable notify dari FEE3
// ===========================================
bool BLEManager::enableNotify(const BLEUUID &serviceUUID, const BLEUUID &charUUID,void (*callback)(BLERemoteCharacteristic*, uint8_t*, size_t, bool))
{
    delay(300);
    BLERemoteCharacteristic *ch = getCharacteristic(serviceUUID, charUUID);
    if (!ch)
        return false;

    BLERemoteDescriptor *desc = ch->getDescriptor(BLEUUID((uint16_t)0x2902));
    if (!desc)
    {
        Serial.println("[BLE] Notify descriptor not found");
        return false;
    }
    uint8_t notifyOn[] = {0x01, 0x00};
    desc->writeValue(notifyOn, sizeof(notifyOn), true);
    Serial.println("[BLE] Notify enabled");
    ch->registerForNotify(callback);
    return true;
}

// ===========================================
// Menulis perintah trigger ke FEE2
// ===========================================
bool BLEManager::writeBytes(const BLEUUID &serviceUUID, const BLEUUID &charUUID, const uint8_t *data, size_t len)
{
    BLERemoteCharacteristic *ch = getCharacteristic(serviceUUID, charUUID);
    if (!ch)
    {
        Serial.println("[BLE] Cannot write bytes, char not found");
        return false;
    }

    ch->writeValue((uint8_t *)data, len);
    Serial.println("[BLE] Write bytes OK");
    return true;
}

// ===========================================
// Perintah trigger sensor
// ===========================================
bool BLEManager::triggerSpO2()
{
    static const uint8_t cmd[] = {0xFE, 0xEA, 0x20, 0x06, 0x6B, 0x00};
    delay(150);
    bool ok = writeBytes(GENERIC_SERVICE, CHAR_WRITE, cmd, sizeof(cmd));
    if (ok)
        Serial.println("[BLE] Trigger SPO2 sent");
    return ok;
}

bool BLEManager::triggerStress()
{
    static const uint8_t cmd[] = {0xFE, 0xEA, 0x20, 0x08, 0xB9, 0x01, 0x00, 0x00};
    delay(150);
    bool ok = writeBytes(GENERIC_SERVICE, CHAR_WRITE, cmd, sizeof(cmd));
    if (ok)
        Serial.println("[BLE] Trigger STRESS sent");
    return ok;
}

// ===========================================
// Callback untuk data BLE masuk
// ===========================================
void BLEManager::notifyThunk(BLERemoteCharacteristic *ch, uint8_t *data, size_t len, bool)
{
    if (len == 0 || !ch)
        return;

    char stress_prefix[] = {0xFE, 0xEA, 0x20, 0x08, 0xB9, 0x11, 0x00};
    char spo2_prefix[] = {0xFE, 0xEA, 0x20, 0x06, 0x6B};
    uint32_t now = millis();

    // check prefix if stress
    if (len == 8 && memcmp(data, stress_prefix, sizeof(stress_prefix)) == 0)
    {
        if (data[7] == 0xFF)
            return;

        instance->Stress.data = data[7];
        instance->Stress.isNew = true;
        instance->Stress.timestamp = now;
        return;
    }
    // check prefix if SpO2
    if (len == 6 && memcmp(data, spo2_prefix, sizeof(spo2_prefix)) == 0)
    {
        if (data[5] == 0xFF)
            return;
        instance->SpO2.data = data[5];
        instance->SpO2.isNew = true;
        instance->SpO2.timestamp = now;
        return;
    }

    Serial.printf("[BLE] Unknown Data,len %d, data :%X \n", len, data);
}

void BLEManager::HRNotifyCallback(BLERemoteCharacteristic *ch, uint8_t *data, size_t len, bool)
{
    // return if not valid
    if (len == 0 || !ch)
        return;
    if (data[1] == 0xFF)
    {
        return;
    }
    uint32_t now = millis();
    instance->HR.data = data[1];
    instance->HR.isNew = true;
    instance->HR.timestamp = now;
}

BLEData BLEManager::getLastSpO2()
{
    BLEData copy = SpO2;
    SpO2.isNew = false;
    return copy;
}
BLEData BLEManager::getLastStress()
{
    BLEData copy = Stress;
    Stress.isNew = false;
    return copy;
}
BLEData BLEManager::getLastHR()
{
    BLEData copy = HR;
    HR.isNew = false;
    return copy;
}

DeviceData BLEManager::BLEDataToSensorData(uint8_t device_id, Topic topic, BLEData data)
{
    DeviceData dev_data;
    dev_data.device_id = device_id;
    dev_data.topic = topic;
    dev_data.sensor.data = data.data;
    dev_data.timestamp = data.timestamp;
    return dev_data;
}