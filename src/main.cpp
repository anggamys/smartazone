#include <Arduino.h>
#include "ble_manager.h"
#include "lora_manager.h"

// BLE target (Heart Rate Monitor)
const char* targetAddress = "f8:fd:e8:84:37:89";
static BLEUUID heartRateService("0000180d-0000-1000-8000-00805f9b34fb");
static BLEUUID heartRateChar("00002a37-0000-1000-8000-00805f9b34fb");

BLEManager ble(targetAddress);

// LoRa pin mapping
#define LORA_NSS   7
#define LORA_SCK   5
#define LORA_MOSI  6
#define LORA_MISO  3
#define LORA_DIO1  33
#define LORA_BUSY  34
#define LORA_RST   8
LoRaHandler lora(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY,
                 LORA_SCK, LORA_MISO, LORA_MOSI);

// HR buffer
static volatile bool hrAvailable = false;
static volatile int hrBuffer     = -1;

void setup() {
    Serial.begin(115200);

    // Init BLE
    ble.begin();
    ble.setHRAvailableFlag((bool*)&hrAvailable, (int*)&hrBuffer);
}

void loop() {
    static bool serviceConnected = false;
    static bool notifyEnabled    = false;
    static bool loraReady        = false;

    // pop BLE logs (aman di loop)
    String logMsg;
    if (ble.popLog(logMsg)) {
        Serial.println(logMsg);
    }

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

    // inisialisasi LoRa setelah BLE connect
    if (!loraReady && ble.isConnected()) {
        if (lora.begin(923.0)) {
            Serial.println("[Main][LoRa] SX1262 siap...");
            loraReady = true;
        } else {
            Serial.println("[Main][LoRa] Gagal inisialisasi LoRa SX1262");
        }
    }

    // kirim HR terbaru
    if (hrAvailable) {
        int hr = hrBuffer;
        hrAvailable = false;
        String msg = "HR:" + String(hr);
        Serial.println("[Main] Current Heart Rate: " + msg);
        if (loraReady) {
            lora.sendMessage(msg);
        }
    }

    delay(100);
}
