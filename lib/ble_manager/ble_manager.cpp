#include "ble_manager.h"
#include <cstring>

// static members
char BLEManager::logBuffer[128] = {0};
bool BLEManager::hasLog = false;
int BLEManager::lastHeartRate = -1;

// HR pointers
static bool *hrFlagPtr = nullptr;
static int *hrBufferPtr = nullptr;

BLEManager::BLEManager(const char *targetAddress, uint32_t scanTime)
    : targetAddress(targetAddress),
      scanTime(scanTime),
      deviceConnected(false),
      lastReconnectAttempt(0),
      pClient(nullptr),
      clientCb(this) {}

void BLEManager::setHRAvailableFlag(bool *flag, int *buffer)
{
    hrFlagPtr = flag;
    hrBufferPtr = buffer;
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
    out = String(logBuffer);
    hasLog = false;
    return true;
}

// Client callbacks
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

// Scan target device
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
            char tmp[128];
            snprintf(tmp, sizeof(tmp), "[BLE] Found target: %s", addr.c_str());
            pushLog(tmp);
            return dev.getAddress();
        }
    }
    pushLog("[BLE] Target not found");
    return BLEAddress("");
}

// Init + scan + connect
void BLEManager::begin(const char *deviceName)
{
    BLEDevice::deinit(true);
    delay(150);
    BLEDevice::init(deviceName);
    delay(100);
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

    bool ok = pClient->connect(addr);
    if (!ok)
    {
        pushLog("[BLE] Failed to connect");
        return false;
    }
    pushLog("[BLE] Connected to server");
    deviceConnected = true;
    return true;
}

bool BLEManager::connectToServiceAndNotify(const BLEUUID &serviceUUID, const BLEUUID &charUUID)
{
    if (!deviceConnected || !pClient)
    {
        pushLog("[BLE] Not connected");
        return false;
    }
    BLERemoteService *svc = pClient->getService(serviceUUID);
    if (!svc)
    {
        pushLog("[BLE] Service not found");
        return false;
    }
    BLERemoteCharacteristic *ch = svc->getCharacteristic(charUUID);
    if (!ch)
    {
        pushLog("[BLE] Char not found");
        return false;
    }
    if (!ch->canNotify())
    {
        pushLog("[BLE] Char cannot notify");
        return false;
    }
    ch->registerForNotify(heartRateNotifyCallback);
    pushLog("[BLE] Notify enabled");
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

// Notify callback
void BLEManager::heartRateNotifyCallback(BLERemoteCharacteristic *, uint8_t *pData, size_t length, bool)
{
    if (length > 1)
    {
        bool is16 = pData[0] & 0x01;
        uint16_t hr = is16 ? (pData[1] | (pData[2] << 8)) : pData[1];
        lastHeartRate = hr;
        if (hrFlagPtr && hrBufferPtr)
        {
            *hrBufferPtr = (int)hr;
            *hrFlagPtr = true;
        }
    }
}
