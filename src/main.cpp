#include <Arduino.h>
#include "ble_manager.h"
#include "lora_manager.h"

// Mode: TX (BLE->LoRa) 1 atau RX (LoRa-only) 0
#define DEVICE_MODE_TX 1
const bool isTransmitter = DEVICE_MODE_TX;

// BLE target & UUID
const char targetAddress[] PROGMEM = "f8:fd:e8:84:37:89";
#define HEART_RATE_SERVICE_UUID "0000180d-0000-1000-8000-00805f9b34fb"
#define HEART_RATE_CHAR_UUID "00002a37-0000-1000-8000-00805f9b34fb"

// BLE manager
BLEManager ble(targetAddress, 6);
static volatile bool hrAvailable = false;
static volatile uint16_t hrBuffer = 0;

// LoRa mapping
static const uint8_t LORA_NSS = 7;
static const uint8_t LORA_SCK = 5;
static const uint8_t LORA_MOSI = 6;
static const uint8_t LORA_MISO = 3;
static const uint8_t LORA_DIO1 = 33;
static const uint8_t LORA_BUSY = 34;
static const uint8_t LORA_RST = 8;

LoRaHandler lora(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY,
                 LORA_SCK, LORA_MISO, LORA_MOSI);

// Timing
struct Timers
{
    uint32_t bleCheck{0};
    uint32_t loraCheck{0};
    uint32_t status{0};
    uint32_t ping{0};
} timers;

static const uint32_t LOOP_DELAY_MS = 100;
static const uint32_t BLE_INTERVAL_MS = 1000;
static const uint32_t LORA_INTERVAL_MS = 50;
static const uint32_t STATUS_INTERVAL_MS = 60000;

// LoRa link
bool loraLinkAlive = false;
uint32_t lastPongMillis = 0;
const char *PING = "PING";
const char *PONG = "PONG";
static const uint32_t PING_INTERVAL_MS = 30000;
static const uint32_t LINK_TTL_MS = 45000;

// BLE log
String bleLog;

void setup()
{
    Serial.begin(115200);
    delay(300);

    if (isTransmitter)
    {
        ble.begin("EoRa-S3");
        ble.setHRAvailableFlag((bool *)&hrAvailable, (int *)&hrBuffer);
        Serial.println(F("[Main] Mode: TRANSMITTER"));
    }
    else
    {
        Serial.println(F("[Main] Mode: RECEIVER"));
    }

    if (!lora.begin(923.0))
    {
        Serial.println(F("[Main] LoRa init failed, halting"));
        while (true)
            delay(1000);
    }
}

void loop()
{
    uint32_t now = millis();

    // Status report
    if (now - timers.status >= STATUS_INTERVAL_MS)
    {
        timers.status = now;
        uint32_t uptime = now / 1000;
        Serial.printf("[Uptime %lu sec] Heap: %u\n", uptime, ESP.getFreeHeap());
    }

    // Transmitter
    if (isTransmitter)
    {
        if (now - timers.bleCheck >= BLE_INTERVAL_MS)
        {
            timers.bleCheck = now;
            ble.tryReconnect();
            ble.connectToServiceAndNotify(BLEUUID(HEART_RATE_SERVICE_UUID),
                                          BLEUUID(HEART_RATE_CHAR_UUID));
        }

        if (now - timers.loraCheck >= LORA_INTERVAL_MS)
        {
            timers.loraCheck = now;

            // 1) kirim HR
            if (hrAvailable)
            {
                hrAvailable = false;
                String payload = "HR:" + String(hrBuffer);
                Serial.println("[TX] " + payload);
                lora.sendMessage(payload);
            }

            // 2) cek RX (PONG)
            String msg;
            int rssi;
            float snr;
            if (lora.receiveMessage(msg, rssi, snr))
            {
                if (msg.equalsIgnoreCase(PONG))
                {
                    loraLinkAlive = true;
                    lastPongMillis = now;
                    Serial.println(F("[TX] Got PONG -> Link alive"));
                }
            }

            // 3) kirim PING kalau link mati
            if (!loraLinkAlive && (now - timers.ping >= PING_INTERVAL_MS))
            {
                timers.ping = now;
                Serial.println(F("[LoRa] Sending PING"));
                lora.sendMessage(String(PING));
            }
        }

        // TTL LoRa
        if (loraLinkAlive && (now - lastPongMillis > LINK_TTL_MS))
        {
            loraLinkAlive = false;
            Serial.println(F("[LoRa] Link expired"));
        }

        // BLE log
        if (ble.popLog(bleLog))
            Serial.println(bleLog);
    }
    // Receiver
    else
    {
        if (now - timers.loraCheck >= LORA_INTERVAL_MS)
        {
            timers.loraCheck = now;
            String msg;
            int rssi;
            float snr;
            if (lora.receiveMessage(msg, rssi, snr))
            {
                Serial.printf("[RX] %s | RSSI:%d SNR:%.1f\n",
                              msg.c_str(), rssi, snr);
                if (msg.equalsIgnoreCase(PING))
                {
                    Serial.println(F("[RX] Replying PONG"));
                    lora.sendMessage(String(PONG));
                }
            }
        }
    }

    delay(LOOP_DELAY_MS);
}
