#include <Arduino.h>
#include "ble_manager.h"
#include "lora_manager.h"

// BLE target
const char* targetAddress = "f8:fd:e8:84:37:89";
static BLEUUID heartRateService("0000180d-0000-1000-8000-00805f9b34fb");
static BLEUUID heartRateChar("00002a37-0000-1000-8000-00805f9b34fb");

BLEManager ble(targetAddress);

// LoRa pin mapping (EoRa-S3 manual)
#define LORA_NSS   7
#define LORA_SCK   5
#define LORA_MOSI  6
#define LORA_MISO  3
#define LORA_DIO1  33
#define LORA_BUSY  34
#define LORA_RST   8

LoRaHandler lora(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, LORA_SCK, LORA_MISO, LORA_MOSI);

void setup() {
    Serial.begin(115200);

    ble.begin();

    if (!lora.begin(923.0)) {
        Serial.println("[Main][LoRa] Gagal inisialisasi LoRa SX1262");
        while (1);
    }
    Serial.println("[Main][LoRa] SX1262 siap...");
}

void loop() {
    ble.loop();

    static bool serviceConnected = false;
    static bool notifyEnabled = false;

    if (ble.isConnected()) {
        if (!serviceConnected) {
            serviceConnected = ble.connectToService(heartRateService);
        }
        if (serviceConnected && !notifyEnabled) {
            notifyEnabled = ble.enableNotify(heartRateService, heartRateChar);
        }

        int hr = ble.getLastHeartRate();
        if (hr != -1) {
            String msg = "HR:" + String(hr);
            Serial.println("[Main] Current Heart Rate: " + msg);
            lora.sendMessage(msg);
        }
    }

    delay(500);
}
