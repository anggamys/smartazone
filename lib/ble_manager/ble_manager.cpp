#include "ble_manager.h"
#include <cstring>

// static
char BLEManager::logBuffer[128] = {0};
bool BLEManager::hasLog = false;
BleData BLEManager::lastBleData = {};

BLEManager::BLEManager(const char *targetAddress, uint32_t scanTime)
    : targetAddress(targetAddress),
      scanTime(scanTime),
      deviceConnected(false),
      lastReconnectAttempt(0),
      pClient(nullptr),
      clientCb(this) {}

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
    parent->deviceConnected = true;
    parent->lastReconnectAttempt = 0;
    pushLog("[BLE] Connected");
}
void BLEManager::MyClientCallback::onDisconnect(BLEClient *)
{
    parent->deviceConnected = false;
    pushLog("[BLE] Disconnected");
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
    delay(500); // beri waktu sebelum enable notify / control
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

bool BLEManager::enableNotify(const BLEUUID &serviceUUID, const BLEUUID &charUUID, bool isHR)
{
    BLERemoteCharacteristic *ch = getCharacteristic(serviceUUID, charUUID);
    if (!ch)
        return false;

    lastBleData.isHeartRate = isHR;
    ch->registerForNotify(&BLEManager::notifyThunk);
    pushLog("[BLE] Notify enabled");
    return true;
}

bool BLEManager::writeControl(const BLEUUID &serviceUUID, const BLEUUID &ctrlUUID, uint8_t value)
{
    BLERemoteCharacteristic *ch = getCharacteristic(serviceUUID, ctrlUUID);
    if (!ch)
        return false;

    // beberapa device tidak set flag write dengan benar → kita tetap coba
    ch->writeValue(&value, 1, true);
    pushLog("[BLE] Control written");
    return true;
}

// callback BLE task: super ringan
void BLEManager::notifyThunk(BLERemoteCharacteristic *, uint8_t *data, size_t len, bool)
{
    if (len == 0)
        return;
    const size_t copyLen = (len > sizeof(lastBleData.payload)) ? sizeof(lastBleData.payload) : len;
    memcpy(lastBleData.payload, data, copyLen);
    lastBleData.length = copyLen;
    lastBleData.timestamp = millis();
    lastBleData.ready = true;
}
