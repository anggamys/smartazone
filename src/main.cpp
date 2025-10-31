#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "ble_manager.h"
#include "lora_manager.h"
#include "mqtt_manager.h"
#include "data.h"

// =============================================
// Konfigurasi utama
// =============================================
#define DEVICE_MODE_TX 1
const bool isTransmitter = DEVICE_MODE_TX;

// BLE target Aolon
const char targetAddress[] PROGMEM = "f8:fd:e8:84:37:89";
#define AOLON_SERVICE_UUID "0000feea-0000-1000-8000-00805f9b34fb"
#define AOLON_WRITE_UUID "0000fee2-0000-1000-8000-00805f9b34fb"
#define AOLON_NOTIFY_UUID "0000fee3-0000-1000-8000-00805f9b34fb"
const int DEVICE_ID = 11;

// BLE & Sensor instance
BLEManager ble(targetAddress, 6);

// LoRa pin mapping
static const uint8_t LORA_NSS = 7;
static const uint8_t LORA_SCK = 5;
static const uint8_t LORA_MOSI = 6;
static const uint8_t LORA_MISO = 3;
static const uint8_t LORA_DIO1 = 33;
static const uint8_t LORA_BUSY = 34;
static const uint8_t LORA_RST = 8;

LoRaHandler lora(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, LORA_SCK, LORA_MISO, LORA_MOSI);

// MQTT setup
const char *WIFI_SSID = "Warkop Kongkoow 2";
const char *WIFI_PASS = "pesanduluyah2";
const char *MQTT_SERVER = "192.168.1.44";
const uint16_t MQTT_PORT = 1883;
const char *MQTT_USER = "mqtt";
const char *MQTT_PASS = "mqttpass";
const char *MQTT_TOPIC = "device/health";
MqttManager mqtt(WIFI_SSID, WIFI_PASS, MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASS);

static const uint32_t BLE_RECONNECT_MS = 5000;
static const uint32_t SEND_INTERVAL_MS = 1000;
static const uint32_t STATUS_INTERVAL_MS = 60000;
static const uint32_t TRIGGER_INTERVAL_MS = 300000;          // 5 menit
static const uint32_t INTERVAL_BETWEEN_SPO2_STRESS = 120000; // 2 menit

// =============================================
// Sinkronisasi waktu (NTP)
// =============================================
time_t bootEpoch = 0;
unsigned long bootMillis = 0;
bool ntpSynced = false;
bool streesTriggerPending = true;

// =============================================
// Timer dan interval
// =============================================
struct Timers
{
    uint32_t bleReconnect{0};
    uint32_t sendTick{0};
    uint32_t status{0};
    uint32_t triggerTick{0};
    uint32_t interval_spo_stress{0};
} timers;

// =============================================
// device data struct
// data can convert to float or lattitude
// =============================================
struct device_data
{
    uint8_t device_id, topic_id;
    uint64_t data, timestamp;
};

struct sensor_data
{
    uint8_t data;
    bool isNew;
    uint32_t timestamp;
};

// =============================================
// encode data for lora message
// =============================================
void EncodeMessage(DeviceData *data, char *msg)
{
    if (sizeof(msg) < sizeof(data))
        return;
    memcpy(msg, data, sizeof(data));
    return;
}

// =============================================
// Parse data for lora message
// =============================================
void ParseMessage(DeviceData *data, char *msg)
{
    if (sizeof(msg) == sizeof(data))
        return;
    memcpy(data, msg, sizeof(msg));
    return;
}

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
        configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
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

time_t getCurrentTime()
{
    if (ntpSynced)
        return bootEpoch + ((millis() - bootMillis) / 1000);
    return millis() / 1000;
}

// =============================================
// Setup
// =============================================
void setup()
{
    Serial.begin(115200);
    delay(300);
    setupTime();

    if (isTransmitter)
    {
        Serial.println(F("[Main] Mode: TRANSMITTER"));
        ble.begin("EoRa-S3");

        if (!lora.begin(923.0))
        {
            Serial.println(F("[Main] LoRa init failed"));
            while (true)
                delay(1000);
        }
        Serial.println(F("[Main] LoRa ready"));
    }
    else
    {
        Serial.println(F("[Main] Mode: RECEIVER"));
        lora.begin(923.0);
        mqtt.begin();
    }
    // for auto start trigger
    timers.triggerTick = -300000;
}

// =============================================
// Loop utama
// =============================================
void loop()
{
    uint32_t now = millis();

    // Status periodik
    if (now - timers.status >= STATUS_INTERVAL_MS)
    {
        timers.status = now;
        Serial.printf("[Status] Uptime:%lus | Heap:%u bytes\n", now / 1000, ESP.getFreeHeap());
    }

    if (isTransmitter)
    {
        BLEData HR, SpO2, Stress;
        // Reconnect BLE jika terputus
        if (now - timers.bleReconnect >= BLE_RECONNECT_MS)
        {
            timers.bleReconnect = now;
            if (!ble.isConnected())
            {
                Serial.println("[BLE] Reconnecting...");
                if (ble.tryReconnect())
                {
                    Serial.println("[BLE] Notify re-enabled after reconnect");
                }
            }
        }

        // Trigger SPO2 dan STRESS setiap 10 detik
        if (ble.isConnected() && (now - timers.triggerTick >= TRIGGER_INTERVAL_MS))
        {
            timers.triggerTick = now;
            Serial.println("[BLE] Triggering SPO2 sensors...");
            ble.triggerSpO2();
            delay(500);
            ble.triggerSpO2();
            timers.interval_spo_stress = now + INTERVAL_BETWEEN_SPO2_STRESS;
            streesTriggerPending = false;
        }
        if (ble.isConnected() && !streesTriggerPending && (now>= timers.interval_spo_stress))
        {
            Serial.println("[BLE] Triggering Stress sensor");
            ble.triggerStress();
            delay(500);
            ble.triggerStress();
            streesTriggerPending = true;
        }

        HR = ble.getLastHR();
        SpO2 = ble.getLastSpO2();
        Stress = ble.getLastStress();

        char msg[sizeof(device_data)];
        DeviceData new_data;
        if (HR.isNew)
        {
            Serial.printf("send hr data: %d \n", HR.data);
            new_data = ble.BLEDataToSensorData(DEVICE_ID, Topic::HEART_RATE, HR);
            EncodeMessage(&new_data, (char *)&msg);
            lora.sendMessage(msg);
        }
        if (SpO2.isNew)
        {
            Serial.printf("send spo2 data: %d \n", SpO2.data);
            new_data = ble.BLEDataToSensorData(DEVICE_ID, Topic::SPO2, SpO2);
            EncodeMessage(&new_data, (char *)&msg);
            lora.sendMessage(msg);
        }
        if (Stress.isNew)
        {
            Serial.printf("send Stress data: %d \n", Stress.data);
            new_data = ble.BLEDataToSensorData(DEVICE_ID, Topic::STRESS, Stress);
            EncodeMessage(&new_data, (char *)&msg);
            lora.sendMessage(msg);
        }
    }
    else
    {
        // Mode RX → terima dan forward ke MQTT
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
