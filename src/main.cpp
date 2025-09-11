#include <Arduino.h>
#include "ble_manager.h"
#include "lora_manager.h"

// Mode: 1 = TX (BLE -> LoRa), 0 = RX (LoRa receiver)
#define DEVICE_MODE_TX 1
const bool isTransmitter = DEVICE_MODE_TX;

// BLE target address
const char targetAddress[] PROGMEM = "f8:fd:e8:84:37:89";

// BLE service/char UUIDs
#define HEART_RATE_SERVICE_UUID "0000180d-0000-1000-8000-00805f9b34fb"
#define HEART_RATE_CHAR_UUID    "00002a37-0000-1000-8000-00805f9b34fb"

// BLE manager (scanTime seconds)
BLEManager ble(targetAddress, 6);

// LoRa pin mapping (EoRa-S3)
static const uint8_t LORA_NSS   = 7;
static const uint8_t LORA_SCK   = 5;
static const uint8_t LORA_MOSI  = 6;
static const uint8_t LORA_MISO  = 3;
static const uint8_t LORA_DIO1  = 33;
static const uint8_t LORA_BUSY  = 34;
static const uint8_t LORA_RST   = 8;

LoRaHandler* lora = nullptr;

// HR buffer for BLE Manager callback
static volatile bool hrAvailable = false;
static volatile uint16_t hrBuffer = 0;

// Timing
static const uint32_t MAIN_LOOP_DELAY = 100;
static const uint32_t BLE_CHECK_INTERVAL = 1000;
static const uint32_t LORA_CHECK_INTERVAL = 50;
static const uint32_t STATUS_REPORT_INTERVAL = 60000;

// LoRa ping/pong
const char LORA_PING[] = "PING";
const char LORA_PONG[] = "PONG";
static bool loraLinkAlive = false;
static uint32_t lastPingMillis = 0;
static uint32_t lastPongMillis = 0;
static const uint32_t LORA_PING_INTERVAL_MS = 30000; // 30s between pings
static const uint32_t LORA_LINK_TTL_MS = 45000; // consider link dead if no pong in 45s

// For logs from BLEManager
String bleLog;

static uint32_t lastBleCheck = 0;
static uint32_t lastLoraCheck = 0;
static uint32_t lastStatusReport = 0;

void handleBLEOperations();
void handleLoRaTransmission();
void handleLoRaReception();
void printSystemStatus();

void setup() {
    Serial.begin(115200);
    delay(300);

    if (isTransmitter) {
        // TX perlu BLE + LoRa
        BLEDevice::deinit(true);
        ble.begin("EoRa-S3");
        ble.setHRAvailableFlag((bool*)&hrAvailable, (int*)&hrBuffer);
        Serial.println(F("[Main] Mode: TRANSMITTER"));
    } else {
        Serial.println(F("[Main] Mode: RECEIVER"));
    }

    // LoRa selalu ada
    lora = new LoRaHandler(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY,
                          LORA_SCK, LORA_MISO, LORA_MOSI);
    if (!lora->begin(923.0)) {
        Serial.println(F("[Main] LoRa init failed, halting"));
        while (true) delay(1000);
    }

    lastBleCheck = millis();
    lastLoraCheck = millis();
    lastStatusReport = millis();

    if (isTransmitter) {
        // TX mulai ping loop
        lastPingMillis = millis() - (LORA_PING_INTERVAL_MS - 2000);
        loraLinkAlive = false;
    }
}

void loop() {
    uint32_t now = millis();

    // status laporan periodik
    if (now - lastStatusReport >= STATUS_REPORT_INTERVAL) {
        printSystemStatus();
        lastStatusReport = now;
    }

    if (isTransmitter) {
        // === TX MODE ===
        if (now - lastBleCheck >= BLE_CHECK_INTERVAL) {
            handleBLEOperations();
            lastBleCheck = now;
        }

        if (now - lastLoraCheck >= LORA_CHECK_INTERVAL) {
            handleLoRaReception();    // cek PONG
            handleLoRaTransmission(); // kirim HR & PING
            lastLoraCheck = now;
        }
    } else {
        // === RX MODE ===
        if (now - lastLoraCheck >= LORA_CHECK_INTERVAL) {
            handleLoRaReception();    // terima HR / PING
            lastLoraCheck = now;
        }
    }

    // cek TTL LoRa (hanya TX butuh)
    if (isTransmitter && loraLinkAlive && (now - lastPongMillis > LORA_LINK_TTL_MS)) {
        Serial.println(F("[LoRa] Link TTL expired -> marking dead"));
        loraLinkAlive = false;
    }

    // flush BLE log (hanya TX)
    if (isTransmitter && ble.popLog(bleLog)) {
        Serial.println(bleLog);
    }

    delay(MAIN_LOOP_DELAY);
}

// BLE tasks: reconnect & enable notify if connected
void handleBLEOperations() {
    static bool serviceConnected = false;
    static bool notifyEnabled = false;

    ble.tryReconnect();

    if (ble.isConnected()) {
        if (!serviceConnected) {
            if (ble.connectToService(BLEUUID(HEART_RATE_SERVICE_UUID))) {
                Serial.println(F("[BLE] Service connected"));
                serviceConnected = true;
            } else {
                Serial.println(F("[BLE] Service connect failed"));
            }
        }
        if (serviceConnected && !notifyEnabled) {
            if (ble.enableNotify(BLEUUID(HEART_RATE_SERVICE_UUID), BLEUUID(HEART_RATE_CHAR_UUID))) {
                Serial.println(F("[BLE] Notifications enabled"));
                notifyEnabled = true;
            } else {
                Serial.println(F("[BLE] Notifications enable failed"));
            }
        }
    } else {
        if (serviceConnected || notifyEnabled) {
            Serial.println(F("[BLE] Disconnected, reset flags"));
        }
        serviceConnected = false;
        notifyEnabled = false;
    }

    // If HR available, we'll send in LoRa transmission path (don't send here synchronously)
}

// LoRa transmit path: send HR immediately if available; send ping occasionally
void handleLoRaTransmission() {
    uint32_t now = millis();

    // 1) If HR available, send immediately (highest priority)
    if (hrAvailable && lora) {
        uint16_t hrVal = hrBuffer;
        hrAvailable = false;

        String payload;
        payload.reserve(16);
        payload = "HR:";
        payload += String(hrVal);

        Serial.print(F("[TX] Heart Rate -> "));
        Serial.println(payload);

        lora->sendMessage(payload);
        // no wait for ack here (we could add sequence/ack later)
    }

    // 2) Ping logic (less frequent). If link not alive, send periodic ping.
    if (!loraLinkAlive && (now - lastPingMillis >= LORA_PING_INTERVAL_MS)) {
        lastPingMillis = now;
        Serial.println(F("[LoRa] Sending PING (link validation)"));
        if (lora) lora->sendMessage(String(LORA_PING));
        // We rely on handleLoRaReception() to capture PONG asynchronously
    }
}

// LoRa receive path: check incoming messages and react
void handleLoRaReception() {
    String msg;
    int rssi = 0;
    float snr = 0.0f;
    if (!lora) return;

    if (lora->receiveMessage(msg, rssi, snr)) {
        // Received something
        if (msg.length() == 0) return;

        Serial.print(F("[RX][LoRa] Received: "));
        Serial.print(msg);
        Serial.print(F(" | RSSI:"));
        Serial.print(rssi);
        Serial.print(F(" dBm | SNR:"));
        Serial.print(snr);
        Serial.println(F(" dB"));

        // If RX mode: reply to PING
        if (!isTransmitter) {
            if (msg.equalsIgnoreCase(LORA_PING)) {
                Serial.println(F("[RX] Got PING -> reply PONG"));
                lora->sendMessage(String(LORA_PONG));
            } else {
                // if HR payload or other:
                if (msg.startsWith("HR:")) {
                    Serial.print(F("[RX] Heart data: "));
                    Serial.println(msg);
                    // further processing e.g. MQTT publish
                }
            }
        } else {
            // TX mode: if receives PONG, mark link alive
            if (msg.equalsIgnoreCase(LORA_PONG)) {
                loraLinkAlive = true;
                lastPongMillis = millis();
                Serial.println(F("[TX] Received PONG -> link alive"));
            } else {
                // Received other messages on TX side (e.g. debug): print
                if (msg.startsWith("HR:")) {
                    // rare: maybe peer sent HR too
                    Serial.print(F("[TX] Unexpected HR received: "));
                    Serial.println(msg);
                }
            }
        }
    }
}

void printSystemStatus() {
    uint32_t uptime = millis() / 1000;
    uint32_t hh = uptime / 3600;
    uint32_t mm = (uptime % 3600) / 60;
    uint32_t ss = uptime % 60;

    Serial.printf("[Uptime: %02lu:%02lu:%02lu]\n", hh, mm, ss);
    Serial.printf("[Free heap: %u bytes]\n", ESP.getFreeHeap());

    if (isTransmitter) {
        Serial.println(F("[Mode: TRANSMITTER]"));
        Serial.printf("[BLE Status: %s]\n", ble.isConnected() ? "CONNECTED" : "DISCONNECTED");
        Serial.printf("[LoRa Link: %s]\n", loraLinkAlive ? "ALIVE" : "DEAD");
        if (ble.isConnected()) {
            Serial.printf("[Last HR (cache): %d bpm]\n", ble.getLastHeartRate());
        }
    } else {
        Serial.println(F("[Mode: RECEIVER]"));
        Serial.println(F("[Status: LISTENING untuk pesan LoRa]"));
    }

    Serial.println(F("[LoRa: READY]"));
}
