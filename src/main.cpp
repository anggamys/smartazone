#include <Arduino.h>
#include "ble_manager.h"
#include "lora_manager.h"
#include "mqtt_manager.h"

// Mode: TX (BLE→LoRa) atau RX (LoRa-only)
#define DEVICE_MODE_TX 0
const bool isTransmitter = DEVICE_MODE_TX;

// Alamat BLE (AOLON Curve)
const char targetAddress[] PROGMEM = "f8:fd:e8:84:37:89";

// Heart Rate Service, Characteristic, Control Point
#define HR_SERVICE_UUID "0000180d-0000-1000-8000-00805f9b34fb"
#define HR_CHAR_UUID "00002a37-0000-1000-8000-00805f9b34fb"          // notify only
#define HR_CONTROL_POINT_UUID "00002a39-0000-1000-8000-00805f9b34fb" // write 0x01 or vendor alt

BLEManager ble(targetAddress, 6);

// LoRa pin mapping
static const uint8_t LORA_NSS = 7;
static const uint8_t LORA_SCK = 5;
static const uint8_t LORA_MOSI = 6;
static const uint8_t LORA_MISO = 3;
static const uint8_t LORA_DIO1 = 33;
static const uint8_t LORA_BUSY = 34;
static const uint8_t LORA_RST = 8;

LoRaHandler lora(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY,
                 LORA_SCK, LORA_MISO, LORA_MOSI);

// MQTT credentials (ganti sesuai server kamu)
const char *WIFI_SSID = "Kopikopen 2";
const char *WIFI_PASS = "kopikopen";
const char *MQTT_SERVER = "192.168.1.205";
const uint16_t MQTT_PORT = 1883;
const char *MQTT_USER = "mqtt";
const char *MQTT_PASS = "mqttpass";
const char *MQTT_TOPIC = "device/heart_rate";

// MQTT manager
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

// log
String bleLog;
uint32_t lastSentTs = 0;

// parser HR 0x2A37
static int parseHeartRate(const uint8_t *data, size_t len)
{
    if (len < 2)
        return -1;
    bool is16 = data[0] & 0x01;
    int hr = is16 && len >= 3 ? (data[1] | (data[2] << 8)) : data[1];
    if (hr < 30 || hr > 220)
        return -1;
    return hr;
}

// hex helper
static String toHexString(const uint8_t *p, size_t n)
{
    String s;
    for (size_t i = 0; i < n; i++)
    {
        if (p[i] < 16)
            s += "0";
        s += String(p[i], HEX);
    }
    s.toUpperCase();
    return s;
}

// kirim trigger start HR ke control point (0x01)
static void triggerHeartRateOnce()
{
    if (!ble.writeControl(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CONTROL_POINT_UUID), 0x01))
    {
        Serial.println("[BLE] HR trigger 0x01 failed");
    }
    else
    {
        Serial.println("[BLE] HR trigger 0x01 sent");
        return;
    }

    delay(120);
    if (!ble.writeControl(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CONTROL_POINT_UUID), 0x15))
    {
        Serial.println("[BLE] HR vendor pre-trigger 0x15 failed");
    }
    else
    {
        Serial.println("[BLE] HR vendor pre-trigger 0x15 sent");
    }

    delay(120);
    if (ble.writeControl(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CONTROL_POINT_UUID), 0x01))
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

        if (!ble.enableNotify(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CHAR_UUID), true))
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

        mqtt.begin(); // inisialisasi MQTT di mode RX
    }
}

void loop()
{
    uint32_t now = millis();

    if (now - timers.status >= STATUS_INTERVAL_MS)
    {
        timers.status = now;
        Serial.printf("[Status] Uptime:%lus | Heap:%u bytes\n", now / 1000, ESP.getFreeHeap());
    }

    if (isTransmitter)
    {
        if (now - timers.bleReconnect >= BLE_RECONNECT_MS)
        {
            timers.bleReconnect = now;
            if (!ble.isConnected())
            {
                Serial.println("[BLE] Trying reconnect...");
                if (ble.tryReconnect())
                {
                    if (ble.enableNotify(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CHAR_UUID), true))
                    {
                        Serial.println("[BLE] Notify re-enabled");
                        delay(300);
                        triggerHeartRateOnce();
                    }
                }
            }
        }

        if (now - timers.sendTick >= SEND_INTERVAL_MS)
        {
            timers.sendTick = now;
            const BleData &d = ble.getLastData();

            if (!d.ready)
            {
                static bool shown = false;
                if (!shown)
                {
                    Serial.println("[BLE] No data yet");
                    shown = true;
                }
            }
            else if (d.timestamp != lastSentTs)
            {
                lastSentTs = d.timestamp;

                int hr = d.isHeartRate ? parseHeartRate(d.payload, d.length) : -1;
                char msg[72];

                if (hr > 0)
                {
                    snprintf(msg, sizeof(msg), "HR,%d,T,%lu", hr, d.timestamp);
                }
                else
                {
                    snprintf(msg, sizeof(msg), "RAW,%s,T,%lu",
                             toHexString(d.payload, d.length).c_str(), d.timestamp);
                }

                lora.sendMessage(String(msg));
                Serial.printf("[LoRa] Sent: %s\n", msg);
                ((BleData &)d).ready = false;
            }
        }

        if (ble.popLog(bleLog))
            Serial.println(bleLog);
    }
    else
    {
        mqtt.loop(); // handle MQTT reconnect / publish queue

        String msg;
        int rssi;
        float snr;

        if (lora.receiveMessage(msg, rssi, snr))
        {
            Serial.printf("[RX] %s | RSSI:%d | SNR:%.1f\n", msg.c_str(), rssi, snr);

            // publish otomatis ke MQTT
            if (mqtt.isConnected())
            {
                mqtt.publish(MQTT_TOPIC, msg);
            }
        }
    }

    delay(50);
}
