#include "ble_manager.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Mutex untuk mencegah race antara notify dan reconnect
static SemaphoreHandle_t bleMutex = xSemaphoreCreateMutex();

char BLEManager::logBuffer[128] = {0};
bool BLEManager::hasLog = false;
BleData BLEManager::lastBleData = {};

BLEManager::BLEManager(const char *targetAddress, uint32_t scanTime)
    : targetAddress(targetAddress),
      scanTime(scanTime),
      deviceConnected(false),
      lastReconnectAttempt(0),
      pClient(nullptr),
      clientCb(this)
{
}

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

    // Beri waktu stack BLE internal menyelesaikan cleanup
    delay(200);
}

BLEAddress BLEManager::scanTarget()
{
    BLEScan *scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    BLEScanResults results = scan->start(scanTime, false);

    String target = String(targetAddress);
    target.toLowerCase();

    for (int i = 0; i < results.getCount(); ++i)
    {
        BLEAdvertisedDevice dev = results.getDevice(i);
        String addr = String(dev.getAddress().toString().c_str());
        addr.toLowerCase();
        if (addr == target)
        {
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
        return false;

    // pastikan client lama dilepas
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

    if (!pClient->connect(addr))
    {
        pushLog("[BLE] Connect failed");
        return false;
    }

    deviceConnected = true;
    pushLog("[BLE] Connected to server");

    // beri waktu BLE stack internal siap sebelum enableNotify
    delay(500);
    return true;
}

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

BLERemoteCharacteristic *BLEManager::getCharacteristic(const BLEUUID &serviceUUID, const BLEUUID &charUUID)
{
    if (!deviceConnected || !pClient)
    {
        pushLog("[BLE] Not connected");
        return nullptr;
    }

    BLERemoteService *svc = pClient->getService(serviceUUID);
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

bool BLEManager::enableNotify(const BLEUUID &serviceUUID, const BLEUUID &charUUID)
{
    // jeda singkat agar service siap setelah connect
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

void BLEManager::notifyThunk(BLERemoteCharacteristic *ch, uint8_t *data, size_t len, bool)
{
    if (len == 0 || !ch)
        return;

    // proteksi akses bersama
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
        else
            lastBleData.serviceUUID = BLEUUID();

        lastBleData.charUUID = ch->getUUID();

        String charStr = lastBleData.charUUID.toString().c_str();
        charStr.toLowerCase();
        lastBleData.isHeartRate = (charStr.indexOf("2a37") != -1);

        if (lastBleData.isHeartRate)
            pushLog("[BLE] Heart Rate data received");
        else
            pushLog("[BLE] Generic data received");

        xSemaphoreGive(bleMutex);
    }
}
