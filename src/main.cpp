#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

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

// BLE target
const char targetAddress[] PROGMEM = "f8:fd:e8:84:37:89";

// Heart Rate Service & Characteristic
#define HR_SERVICE_UUID "0000180d-0000-1000-8000-00805f9b34fb"
#define HR_CHAR_UUID "00002a37-0000-1000-8000-00805f9b34fb"

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
const char *WIFI_SSID = "Warkop Kongkoow 2";
const char *WIFI_PASS = "pesanduluyah2";
const char *MQTT_SERVER = "192.168.1.44";
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

// Cache sensor terakhir yang valid
SensorData lastValidSensor = {"NONE", 0, 0, false};

// Waktu sinkronisasi
time_t bootEpoch = 0;
unsigned long bootMillis = 0;
bool ntpSynced = false;

// Sinkronisasi waktu NTP sekali saat startup
void setupTime()
{
    Serial.print("[Time] Connecting WiFi for NTP...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n[Time] WiFi connected");
        configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // WIB
        delay(2000);

        time_t now;
        if (time(&now))
        {
            bootEpoch = now;
            bootMillis = millis();
            ntpSynced = true;
            Serial.printf("[Time] NTP sync success: %lu\n", (unsigned long)now);
        }
        else
        {
            Serial.println("[Time] NTP sync failed");
        }

        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
    else
    {
        Serial.println("\n[Time] WiFi not connected, fallback to millis()");
    }
}

// Ambil waktu epoch (real atau fallback)
time_t getCurrentTime()
{
    if (ntpSynced)
        return bootEpoch + ((millis() - bootMillis) / 1000);
    else
        return millis() / 1000;
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    setupTime();

    if (isTransmitter)
    {
        Serial.println(F("[Main] Mode: TRANSMITTER"));
        ble.begin("EoRa-S3");

        // Enable notify HR characteristic
        if (!ble.enableNotify(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CHAR_UUID)))
        {
            Serial.println(F("[Main] Enable HR notify failed"));
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

    // status log periodik
    if (now - timers.status >= STATUS_INTERVAL_MS)
    {
        timers.status = now;
        Serial.printf("[Status] Uptime:%lus | Heap:%u bytes\n", now / 1000, ESP.getFreeHeap());
    }

    // TX MODE: BLE → LoRa
    if (isTransmitter)
    {
        // Reconnect BLE jika terputus
        if (now - timers.bleReconnect >= BLE_RECONNECT_MS)
        {
            timers.bleReconnect = now;
            if (!ble.isConnected())
            {
                Serial.println("[BLE] Trying reconnect...");
                if (ble.tryReconnect())
                {
                    delay(1000); // jeda aman sebelum enable notify
                    if (ble.enableNotify(BLEUUID(HR_SERVICE_UUID), BLEUUID(HR_CHAR_UUID)))
                        Serial.println("[BLE] Notify re-enabled after reconnect");
                    else
                        Serial.println("[BLE] Enable notify failed after reconnect");
                }
            }
        }

        // Kirim data periodik
        if (now - timers.sendTick >= SEND_INTERVAL_MS)
        {
            timers.sendTick = now;
            const BleData &raw = ble.getLastData();

            // Jika ada data baru
            if (raw.ready)
            {
                sensor.updateFromBle(raw);
                ((BleData &)raw).ready = false;
            }

            const SensorData &s = sensor.getData();

            // Simpan data valid terbaru
            if (s.valid)
                lastValidSensor = s;

            // Gunakan cache jika belum ada data baru
            if (lastValidSensor.valid)
            {
                time_t currentTime = getCurrentTime();

                // Format waktu lokal (YYYY-MM-DD HH:MM:SS)
                struct tm timeinfo;
                localtime_r(&currentTime, &timeinfo);
                char timeStr[20];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

                String msg = lastValidSensor.type + "," +
                             String(lastValidSensor.value, 1) +
                             ",T," + String(timeStr);

                lora.sendMessage(msg);
                Serial.println("[LoRa] Sent (cached): " + msg);
            }
            else
            {
                Serial.println("[BLE] No valid data to send");
            }

            if (ble.popLog(bleLog))
                Serial.println(bleLog);
        }
    }

    // RX MODE: LoRa → MQTT
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
                mqtt.publish(MQTT_TOPIC, msg);
        }
    }

    delay(50);
}
