#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <TinyGPSPlus.h>
#include "lora_manager.h"
#include "mqtt_manager.h"
#include "data.h"

#ifdef DEVICE_MODE_CLIENT
#elif defined(DEVICE_MODE_BASE)
#include <AsyncHTTPRequest_Generic.h>   
#include "ArduinoJson.h"
#endif

#define LED_PIN GPIO_NUM_37

#ifdef DEVICE_MODE_CLIENT
#include "ble_manager.h"
// BLE target Aolon
const int DEVICE_ID = 11;
const char targetAddress[] PROGMEM = "f8:fd:e8:84:37:89";
#define AOLON_SERVICE_UUID "0000feea-0000-1000-8000-00805f9b34fb"
#define AOLON_WRITE_UUID "0000fee2-0000-1000-8000-00805f9b34fb"
#define AOLON_NOTIFY_UUID "0000fee3-0000-1000-8000-00805f9b34fb"

#define GPS_BAUD 9600
HardwareSerial GPSSerial(2);
TinyGPSPlus gps;

static const uint32_t BLE_RECONNECT_MS = 5000;
static const uint32_t TRIGGER_INTERVAL_MS = 300000;          // 5 menit
static const uint32_t INTERVAL_BETWEEN_SPO2_STRESS = 120000; // 2 menit
static const uint32_t GPS_INTERVAL_MS = 60000;               // 1 menit
bool streesTriggerPending = true;
GPSData gpsData;

struct Timers
{
    uint32_t bleReconnect{0};
    uint32_t sendTick{0};
    uint32_t status{0};
    uint32_t triggerTick{0};
    uint32_t interval_spo_stress{0};
    uint32_t gps_tick{0};
} timers;

// BLE & Sensor instance
BLEManager ble(targetAddress, 6);

// =============================================
// GPS Task
// =============================================
void gps_task(void *pvParameters)
{
    GPSData *data = (GPSData *)pvParameters;
    GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPIO_NUM_43, GPIO_NUM_44); // RX, TX
    while (true)
    {
        uint32_t now = millis();
        // while (GPSSerial.available())
        // {
        //     char c = GPSSerial.read();
        //     gps.encode(c);
        // }
        // if (gps.location.isUpdated())
        // {
        //     if (!gps.location.isValid())
        //     {
        //         Serial.println("[GPS] Location invalid");
        //         continue;
        //     }
        //     if (now - timers.gps_tick < GPS_INTERVAL_MS)
        //         continue;
        //     timers.gps_tick = now;
        //     data->lattitude = gps.location.lat();
        //     data->longitude = gps.location.lng();
        //     data->isNew = true;
        //     Serial.printf("[GPS] New location: %.6f, %.6f\n", data->lattitude, data->longitude);
        // }
        if (now - timers.gps_tick < GPS_INTERVAL_MS)
            continue;
        timers.gps_tick = now;
        data->lattitude = -7.33468736879843;
        data->longitude = 112.78816294422748;
        data->isNew = true;
        Serial.printf("[GPS] New location: %.6f, %.6f\n", data->lattitude, data->longitude);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
#endif
std::string TopictoString(Topic topic)
{
    switch (topic)
    {
    case Topic::HEART_RATE:
        return "heart_rate";
    case Topic::SPO2:
        return "spo2";
    case Topic::STRESS:
        return "stress";
    case Topic::GPS:
        return "GPS";
    default:
        return "unknown";
    }
}

#ifdef DEVICE_MODE_BASE
// MQTT setup
const char *WIFI_SSID = "teknik";
const char *WIFI_PASS = "sipalingteknik";
const char *MQTT_SERVER = "192.168.1.44";
const uint16_t MQTT_PORT = 1883;
const char *MQTT_USER = "mqtt";
const char *MQTT_PASS = "mqttpass";
const char *MQTT_TOPIC = "device/health";
const char *API_URL = "http://10.107.116.40:8000/api/update-log";
MqttManager mqtt(WIFI_SSID, WIFI_PASS, MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASS);
AsyncHTTPRequest request;

// =============================================
// Sinkronisasi waktu (NTP)
// =============================================
time_t bootEpoch = 0;
unsigned long bootMillis = 0;
bool ntpSynced = false;

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
    }
    else
    {
        Serial.println("\n[Time] WiFi not connected, fallback to millis()");
    }
}

void PostDeviceData(const DeviceData &data){
    static bool requestOpenResult = false;
    StaticJsonDocument<256> doc;
    doc["device_id"] = data.device_id;
    if (data.topic == Topic::GPS)
    {
        doc["lattitude"] = data.sensor.location.lattitude;
        doc["longitude"] = data.sensor.location.longitude;
    }
    else if (data.topic == Topic::HEART_RATE )
    {
        doc["heart_rate"] = data.sensor.value;
    }
    else if (data.topic == Topic::SPO2 )
    {
        doc["spo2"] = data.sensor.value;
    }
    else if (data.topic == Topic::STRESS )
    {
        doc["stress_level"] = data.sensor.value;
    }
    String json;
    serializeJson(doc, json);
    if (request.readyState() == readyStateUnsent || request.readyState() == readyStateDone){
        requestOpenResult =  request.open("POST", API_URL);
        request.setReqHeader("Content-Type", "application/json");
        if (!requestOpenResult){
            Serial.println("[HTTP] Failed to open request");
            return;
        }else{
            request.send(json);
            Serial.println("[HTTP] Posting data: " + json);
        }
    }else{
        Serial.println("[HTTP] Request busy, skipping...");
    }
}

void requestCallback(void *optParm,AsyncHTTPRequest* request, int readyState)
{
    (void) optParm;
    if (readyState == readyStateDone)
    {
        int status = request->responseHTTPcode();
        String response = request->responseText();
        if (status == 200|| status == 201)
        {
            Serial.println("[HTTP] Response: " + response);
        }
        else
        {
            Serial.printf("[HTTP] Error: status code %d\n", status );
            Serial.println(response);
        }
    }
}

struct Timers
{
    uint32_t status{0};
} timers;

time_t getCurrentTime()
{
    if (ntpSynced)
        return bootEpoch + ((millis() - bootMillis) / 1000);
    return millis() / 1000;
}
#endif

// LoRa pin mapping
static const uint8_t LORA_NSS = 7;
static const uint8_t LORA_SCK = 5;
static const uint8_t LORA_MOSI = 6;
static const uint8_t LORA_MISO = 3;
static const uint8_t LORA_DIO1 = 33;
static const uint8_t LORA_BUSY = 34;
static const uint8_t LORA_RST = 8;

LoRaHandler lora(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, LORA_SCK, LORA_MISO, LORA_MOSI);
static const uint32_t STATUS_INTERVAL_MS = 60000;
#ifdef DEVICE_MODE_BASE

#endif

// =============================================
// Setup
// =============================================
void setup()
{
    Serial.begin(115200);
    delay(300);
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

#ifdef DEVICE_MODE_CLIENT
    Serial.println(F("[Main] Mode: CLIENT"));
    ble.begin("EoRa-S3");
    if (!lora.begin(923.0))
    {
        Serial.println(F("[Main] LoRa init failed"));
        while (true)
            delay(1000);
    }
    Serial.println(F("[Main] LoRa ready"));
    // for auto start trigger
    timers.triggerTick = -300000;
    // Start GPS task
    xTaskCreatePinnedToCore(
        gps_task,         /* Task function. */
        "GPS Task",       /* name of task. */
        4096,             /* Stack size of task */
        (void *)&gpsData, /* parameter of the task */
        1,                /* priority of the task */
        NULL,             /* Task handle to keep track of created task */
        1);               /* pin task to core 1 */

#elif defined(DEVICE_MODE_BASE)
    setupTime();
    Serial.println(F("[Main] Mode: BASE"));
    lora.begin(923.0);
    request.setDebug(false);
    request.onReadyStateChange(requestCallback);
    // mqtt.begin();
#endif
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

#ifdef DEVICE_MODE_CLIENT
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
    if (ble.isConnected() && !streesTriggerPending && (now >= timers.interval_spo_stress))
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
    DeviceData new_data;
    if (HR.isNew)
    {
        Serial.printf("send hr data: %d \n", HR.data);
        new_data = ble.BLEDataToSensorData(DEVICE_ID, Topic::HEART_RATE, HR);
        lora.transmit((uint8_t*)&new_data, sizeof(DeviceData));
    }
    if (SpO2.isNew)
    {
        Serial.printf("send spo2 data: %d \n", SpO2.data);
        new_data = ble.BLEDataToSensorData(DEVICE_ID, Topic::SPO2, SpO2);
        lora.transmit((uint8_t*)&new_data, sizeof(DeviceData));
    }
    if (Stress.isNew)
    {
        Serial.printf("send Stress data: %d \n", Stress.data);
        new_data = ble.BLEDataToSensorData(DEVICE_ID, Topic::STRESS, Stress);
        lora.transmit((uint8_t*)&new_data, sizeof(DeviceData));
    }
    if (gpsData.isNew)
    {
        gpsData.isNew = false;
        Serial.printf("send GPS data: (%.6f, %.6f) \n", gpsData.lattitude, gpsData.longitude);
        new_data = DeviceData();
        new_data.device_id = DEVICE_ID;
        new_data.topic = Topic::GPS;
        new_data.sensor.location.lattitude = gpsData.lattitude;
        new_data.sensor.location.longitude = gpsData.longitude;
        lora.transmit((uint8_t*)&new_data, sizeof(DeviceData));
    }
#elif defined(DEVICE_MODE_BASE)
    // Mode RX → terima dan forward ke MQTT
    // mqtt.loop();
    String msg;
    DeviceData device_data;
    struct tm timeinfo;
    char timeStringBuff[64];
    String mqtt_payload;
    std::string full_topic;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    if (lora.receive())
    {
        receivedPacket packet = lora.getNewPacket();
        if (!packet.isNew)
            return;
        device_data = packet.device_data;
        if (device_data.topic != Topic::GPS)
        {
            std::string topic_str = TopictoString(device_data.topic);
            Serial.printf("[LORA] get data from device: %d on Topic : %s and value: %d\n at %s\n", device_data.device_id, topic_str.c_str(), device_data.sensor.value, timeStringBuff);
            mqtt_payload = String(device_data.sensor.value);
            full_topic = std::to_string(device_data.device_id) + "/" + TopictoString(device_data.topic);
        }
        else
        {
            Serial.printf("[LORA] get data from device: %d on Topic : %s and location: (%.6f, %.6f) at %s\n", device_data.device_id, TopictoString(device_data.topic).c_str(), device_data.sensor.location.lattitude, device_data.sensor.location.longitude, timeStringBuff);
            mqtt_payload = String("{\"lattitude\":") + String(device_data.sensor.location.lattitude, 6) + String(", \"longitude\":") + String(device_data.sensor.location.longitude, 6) + String("}");
            full_topic = std::to_string(device_data.device_id) + "/" + TopictoString(device_data.topic);
        }
        PostDeviceData(device_data);
        // if (mqtt.isConnected())
        //     mqtt.publish((char *)full_topic.c_str(), mqtt_payload);
    }
#endif
    delay(50);
}
