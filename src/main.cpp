#include <Arduino.h>
#include "ble_manager.h"
#include "lora_manager.h"
#include "mqtt_manager.h"
#include "sensor.h"

// Helper konversi payload ke hex
static String toHexString(const uint8_t *data, size_t len)
{
    String s;
    s.reserve(len * 2);
    const char hexChars[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; ++i)
    {
        uint8_t v = data[i];
        s += hexChars[(v >> 4) & 0x0F];
        s += hexChars[v & 0x0F];
    }
    return s;
}

// Mode: TX (BLE→LoRa) atau RX (LoRa→MQTT)
#define DEVICE_MODE_TX 1
const bool isTransmitter = DEVICE_MODE_TX;

// BLE Target (AOLON Curve)
const char targetAddress[] PROGMEM = "f8:fd:e8:84:37:89";

// Heart Rate Service & Characteristic
#define HR_SERVICE_UUID "0000180d-0000-1000-8000-00805f9b34fb"
#define HR_CHAR_UUID "00002a37-0000-1000-8000-00805f9b34fb"          // notify only
#define HR_CONTROL_POINT_UUID "00002a39-0000-1000-8000-00805f9b34fb" // write 0x01 (start measurement)

BLEManager ble(targetAddress, 6);
SensorManager sensor;

// LoRa pin mapping
static const uint8_t LORA_NSS = 7;
static const uint8_t LORA_SCK = 5;
static const uint8_t LORA_MOSI = 6;
static const uint8_t LORA_MISO = 3;
static const uint8_t LORA_DIO1 = 33;
static const uint8_t LORA_BUSY = 34;
static const uint8_t LORA_RST = 8;

LoRaHandler lora(
    LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY,
    LORA_SCK, LORA_MISO, LORA_MOSI);

// MQTT credentials
const char *WIFI_SSID = "Kopikopen 2";
const char *WIFI_PASS = "kopikopen";
const char *MQTT_SERVER = "192.168.1.205";
const uint16_t MQTT_PORT = 1883;
const char *MQTT_USER = "mqtt";
const char *MQTT_PASS = "mqttpass";
const char *MQTT_TOPIC = "device/heart_rate";

MqttManager mqtt(WIFI_SSID, WIFI_PASS, MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASS);

// Timer
struct Timers
{
    uint32_t bleReconnect{0};
    uint32_t sendTick{0};
    uint32_t status{0};
} timers;

static const uint32_t BLE_RECONNECT_MS = 3000;
static const uint32_t SEND_INTERVAL_MS = 1000;
static const uint32_t STATUS_INTERVAL_MS = 60000;

// Log
String bleLog;
uint32_t lastSentTs = 0;

// Trigger start heart rate measurement
static void triggerHeartRateOnce()
{
    if (!ble.writeByte(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CONTROL_POINT_UUID), 0x01))
    {
        Serial.println("[BLE] HR trigger 0x01 failed");
    }
    else
    {
        Serial.println("[BLE] HR trigger 0x01 sent");
        return;
    }

    delay(120);
    if (!ble.writeByte(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CONTROL_POINT_UUID), 0x15))
    {
        Serial.println("[BLE] HR vendor pre-trigger 0x15 failed");
    }
    else
    {
        Serial.println("[BLE] HR vendor pre-trigger 0x15 sent");
    }

    delay(120);
    if (ble.writeByte(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CONTROL_POINT_UUID), 0x01))
    {
        Serial.println("[BLE] HR trigger 0x01 re-sent");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    if (isTransmitter)
    {
        Serial.println(F("[Main] Mode: TRANSMITTER"));
        ble.begin("EoRa-S3");

        // enable notify HR
        if (!ble.enableNotify(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CHAR_UUID)))
        {
            Serial.println(F("[Main] Enable HR notify failed"));
        }
        else
        {
            delay(300);
            triggerHeartRateOnce();
        }

        if (!lora.begin(923.0))
        {
            Serial.println(F("[Main] LoRa init failed, halting"));
            while (true)
                delay(1000);
        }
        Serial.println(F("[Main] LoRa ready"));
    }
    else
    {
        Serial.println(F("[Main] Mode: RECEIVER"));

        if (!lora.begin(923.0))
        {
            Serial.println(F("[Main] LoRa RX init failed"));
            while (true)
                delay(1000);
        }

        mqtt.begin();
    }
}

void loop()
{
    uint32_t now = millis();

    // Status
    if (now - timers.status >= STATUS_INTERVAL_MS)
    {
        timers.status = now;
        Serial.printf("[Status] Uptime:%lus | Heap:%u bytes\n", now / 1000, ESP.getFreeHeap());
    }

    // TX MODE → BLE + LoRa
    if (isTransmitter)
    {
        // Reconnect BLE
        if (now - timers.bleReconnect >= BLE_RECONNECT_MS)
        {
            timers.bleReconnect = now;
            if (!ble.isConnected())
            {
                Serial.println("[BLE] Trying reconnect...");
                if (ble.tryReconnect())
                {
                    if (ble.enableNotify(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CHAR_UUID)))
                    {
                        Serial.println("[BLE] Notify re-enabled");
                        delay(300);
                        triggerHeartRateOnce();
                    }
                }
            }
        }

        // Kirim data periodik
        if (now - timers.sendTick >= SEND_INTERVAL_MS)
        {
            timers.sendTick = now;
            const BleData &raw = ble.getLastData();

            if (!raw.ready)
            {
                static bool shown = false;
                if (!shown)
                {
                    Serial.println("[BLE] No data yet");
                    shown = true;
                }
            }
            else
            {
                String msg = "RAW," + toHexString(raw.payload, raw.length) + ",T," + String(raw.timestamp);
                lastSentTs = raw.timestamp;

                // Proses data melalui SensorManager
                sensor.updateFromBle(raw);
                const SensorData &s = sensor.getData();

                if (s.valid)
                {
                    msg = s.type + "," + String(s.value, 1) + ",T," + String(s.timestamp);
                }

                lora.sendMessage(msg);
                Serial.println("[LoRa] Sent: " + msg);
                ((BleData &)raw).ready = false;
            }

            // BLE log
            if (ble.popLog(bleLog))
                Serial.println(bleLog);
        }
    }
    // RX MODE → LoRa → MQTT
    else
    {
        mqtt.loop();

        String msg;
        int rssi;
        float snr;

        if (lora.receiveMessage(msg, rssi, snr))
        {
            Serial.printf("[RX] %s | RSSI:%d | SNR:%.1f\n", msg.c_str(), rssi, snr);

            if (mqtt.isConnected())
            {
                mqtt.publish(MQTT_TOPIC, msg);
            }
        }
    }

    delay(50);
}
