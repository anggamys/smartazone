#include <Arduino.h>
#include "ble_manager.h"
#include "lora_manager.h"
// #include "mqtt_manager.h"

// Config mode: TX or RX
#define DEVICE_MODE_TX   0   // 1 = transmitter (BLE->LoRa), 0 = receiver (LoRa->MQTT)
const bool isTransmitter = DEVICE_MODE_TX;

// BLE target (Heart Rate Monitor)
const char* targetAddress = "f8:fd:e8:84:37:89";
static BLEUUID heartRateService("0000180d-0000-1000-8000-00805f9b34fb");
static BLEUUID heartRateChar("00002a37-0000-1000-8000-00805f9b34fb");
BLEManager ble(targetAddress);

// LoRa pin mapping (EoRa-S3)
#define LORA_NSS   7
#define LORA_SCK   5
#define LORA_MOSI  6
#define LORA_MISO  3
#define LORA_DIO1  33
#define LORA_BUSY  34
#define LORA_RST   8
LoRaHandler lora(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY,
                 LORA_SCK, LORA_MISO, LORA_MOSI);

// MQTT config
// const char* WIFI_SSID   = "YourSSID";
// const char* WIFI_PASS   = "YourPassword";
// const char* MQTT_BROKER = "test.mosquitto.org";  
// const uint16_t MQTT_PORT = 1883;
// MQTTManager mqtt(WIFI_SSID, WIFI_PASS, MQTT_BROKER, MQTT_PORT);

// Buffer HR dari BLE ke LoRa
static volatile bool hrAvailable = false;
static volatile int hrBuffer     = -1;

// Setup
void setup() {
    Serial.begin(115200);

    if (isTransmitter) {
        // TX Mode → BLE + LoRa
        ble.begin();
        ble.setHRAvailableFlag((bool*)&hrAvailable, (int*)&hrBuffer);

        if (!lora.begin(923.0)) {
            Serial.println("[Main][LoRa] Gagal inisialisasi SX1262 (TX)");
            while (true) delay(100);
        }

        Serial.println("[Main][LoRa] TX siap...");
    } else {
        // RX Mode → LoRa + MQTT
        Serial.println("[Main] Inisialisasi LoRa RX...");
        
        if (!lora.begin(923.0)) {
            Serial.println("[Main][LoRa] Gagal inisialisasi SX1262 (RX)");
            while (true) delay(100);
        }

        Serial.println("[Main][LoRa] RX siap...");

        // mqtt.begin();
        // mqtt.subscribe("lora/downlink", [](String msg) {
        //     Serial.println("[MQTT] Downlink: " + msg);
        //     // bisa diteruskan ke LoRa
        //     lora.sendMessage("DOWN:" + msg);
        // });
    }
}

// Loop
void loop() {
    static bool serviceConnected = false;
    static bool notifyEnabled    = false;

    if (isTransmitter) {
        // TX Mode
        // BLE reconnect handler
        ble.tryReconnect();

        // connect ke service + notify
        if (ble.isConnected()) {
            if (!serviceConnected) {
                serviceConnected = ble.connectToService(heartRateService);
            }
            if (serviceConnected && !notifyEnabled) {
                notifyEnabled = ble.enableNotify(heartRateService, heartRateChar);
            }
        }

        // kirim HR terbaru via LoRa
        if (hrAvailable) {
            int hr = hrBuffer;
            hrAvailable = false;
            String msg = "HR:" + String(hr);
            Serial.println("[TX] Heart Rate: " + msg);
            lora.sendMessage(msg);
        }

    } else {
        // RX Mode
        // mqtt.loop();

        String msg;
        int rssi;
        float snr;
        if (lora.receiveMessage(msg, rssi, snr)) {
            Serial.println("[RX][LoRa] " + msg + " | RSSI:" + String(rssi));
            // mqtt.publish("lora/uplink", msg);
        }
    }

    delay(100);
}
