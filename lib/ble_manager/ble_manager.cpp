#include "ble_manager.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Mutex untuk mencegah race condition antar thread BLE
static SemaphoreHandle_t bleMutex = xSemaphoreCreateMutex();

// Static storage
char BLEManager::logBuffer[128] = {0};
bool BLEManager::hasLog = false;
BleData BLEManager::lastBleData = {};

// UUID service & characteristic Aolon Curve
static const BLEUUID SERVICE_AOLON("0000feea-0000-1000-8000-00805f9b34fb");
static const BLEUUID CHAR_WRITE("0000fee2-0000-1000-8000-00805f9b34fb");
static const BLEUUID CHAR_NOTIFY("0000fee3-0000-1000-8000-00805f9b34fb");

BLEManager::BLEManager(const char *targetAddress, uint32_t scanTime)
    : targetAddress(targetAddress),
      scanTime(scanTime),
      deviceConnected(false),
      lastReconnectAttempt(0),
      pClient(nullptr),
      clientCb(this)
{
}

// ===========================================
// Utilitas log
// ===========================================
void BLEManager::pushLog(const char *msg)
{
    strncpy(logBuffer, msg, sizeof(logBuffer) - 1);
    logBuffer[sizeof(logBuffer) - 1] = '\0';
    hasLog = true;
}

bool BLEManager::popLog(String &out)
{
    if (!hasLog)
        return false;
    out = logBuffer;
    hasLog = false;
    return true;
}

// ===========================================
// Callback koneksi
// ===========================================
void BLEManager::MyClientCallback::onConnect(BLEClient *)
{
    parent_->deviceConnected = true;
    parent_->lastReconnectAttempt = 0;
    pushLog("[BLE] Connected");
}

void BLEManager::MyClientCallback::onDisconnect(BLEClient *)
{
    parent_->deviceConnected = false;
    pushLog("[BLE] Disconnected");
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
            pushLog("[BLE] Found target");
            return dev.getAddress();
        }
    }

    pushLog("[BLE] Target not found");
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
        pushLog("[BLE] Device not found during scan");
        return false;
    }

    if (pClient)
    {
        if (pClient->isConnected())
            return true;
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
    }

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(&clientCb);

    Serial.printf("[BLE] Connecting to %s...\n", addr.toString().c_str());
    if (!pClient->connect(addr))
    {
        pushLog("[BLE] Connect failed");
        return false;
    }

    deviceConnected = true;
    pushLog("[BLE] Connected to server");

    // Tampilkan daftar service untuk debug
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

    delay(500);
    enableNotify(SERVICE_AOLON, CHAR_NOTIFY);

    delay(300);
    triggerSpO2();
    delay(200);
    triggerStress();

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
    if (!deviceConnected || !pClient)
    {
        pushLog("[BLE] Not connected");
        return nullptr;
    }

    BLERemoteService *svc = nullptr;
    for (int i = 0; i < 3 && !svc; i++)
    {
        svc = pClient->getService(serviceUUID);
        if (!svc)
            delay(200);
    }

    if (!svc)
    {
        pushLog("[BLE] Service not found");
        return nullptr;
    }

    BLERemoteCharacteristic *ch = svc->getCharacteristic(charUUID);
    if (!ch)
    {
        pushLog("[BLE] Char not found");
        return nullptr;
    }

    return ch;
}

// ===========================================
// Enable notify dari FEE3
// ===========================================
bool BLEManager::enableNotify(const BLEUUID &serviceUUID, const BLEUUID &charUUID)
{
    delay(300);
    BLERemoteCharacteristic *ch = getCharacteristic(serviceUUID, charUUID);
    if (!ch)
        return false;

    lastBleData.serviceUUID = serviceUUID;
    lastBleData.charUUID = charUUID;
    ch->registerForNotify(&BLEManager::notifyThunk);
    pushLog("[BLE] Notify enabled");
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
        pushLog("[BLE] Write failed (char not found)");
        return false;
    }

    ch->writeValue((uint8_t *)data, len, true);
    pushLog("[BLE] Write bytes OK");
    return true;
}

// ===========================================
// Perintah trigger sensor
// ===========================================
bool BLEManager::triggerSpO2()
{
    static const uint8_t cmd[] = {0xFE, 0xEA, 0x20, 0x06, 0x6B, 0x00};
    delay(150);
    bool ok = writeBytes(SERVICE_AOLON, CHAR_WRITE, cmd, sizeof(cmd));
    if (ok)
        pushLog("[BLE] Trigger SPO2 sent");
    return ok;
}

bool BLEManager::triggerStress()
{
    static const uint8_t cmd[] = {0xFE, 0xEA, 0x20, 0x08, 0xB9, 0x01, 0x00, 0x00};
    delay(150);
    bool ok = writeBytes(SERVICE_AOLON, CHAR_WRITE, cmd, sizeof(cmd));
    if (ok)
        pushLog("[BLE] Trigger STRESS sent");
    return ok;
}

// ===========================================
// Callback untuk data BLE masuk
// ===========================================
void BLEManager::notifyThunk(BLERemoteCharacteristic *ch, uint8_t *data, size_t len, bool)
{
    if (len == 0 || !ch)
        return;

    if (xSemaphoreTake(bleMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        const size_t copyLen = min(len, sizeof(lastBleData.payload));
        memcpy(lastBleData.payload, data, copyLen);
        lastBleData.length = copyLen;
        lastBleData.timestamp = millis();
        lastBleData.ready = true;

        BLERemoteService *svc = ch->getRemoteService();
        if (svc)
            lastBleData.serviceUUID = svc->getUUID();

        lastBleData.charUUID = ch->getUUID();

        // deteksi jenis data berdasarkan pola payload
        if (len >= 6 && data[2] == 0x20)
        {
            if (data[3] == 0x06)
                pushLog("[BLE] SPO2 data received");
            else if (data[3] == 0x08)
                pushLog("[BLE] Stress data received");
            else
                pushLog("[BLE] Generic FEE3 data");
        }
        else
        {
            pushLog("[BLE] Unknown data");
        }

        xSemaphoreGive(bleMutex);
    }
}
