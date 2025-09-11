#include <Arduino.h>
#include "ble_manager.h"
#include "lora_manager.h"
// #include "mqtt_manager.h"

// Function declarations
void handleBLEOperations();
void handleLoRaTransmission();
void handleLoRaReception();
void printSystemStatus();

// Config mode: TX or RX
#define DEVICE_MODE_TX   1   // 1 = transmitter (BLE->LoRa), 0 = receiver (LoRa->MQTT)
const bool isTransmitter = DEVICE_MODE_TX;

// BLE target (Heart Rate Monitor) - use PROGMEM for string constants
const char targetAddress[] PROGMEM = "f8:fd:e8:84:37:89";

// Use string literals instead of BLEUUID objects to save memory
#define HEART_RATE_SERVICE_UUID "0000180d-0000-1000-8000-00805f9b34fb"
#define HEART_RATE_CHAR_UUID    "00002a37-0000-1000-8000-00805f9b34fb"

// Initialize BLE manager with smaller scan time for faster connection
BLEManager ble(targetAddress, 3); // Reduced scan time from default 5 to 3 seconds

// LoRa pin mapping (EoRa-S3) - use const to save RAM
static const uint8_t LORA_NSS   = 7;
static const uint8_t LORA_SCK   = 5;
static const uint8_t LORA_MOSI  = 6;
static const uint8_t LORA_MISO  = 3;
static const uint8_t LORA_DIO1  = 33;
static const uint8_t LORA_BUSY  = 34;
static const uint8_t LORA_RST   = 8;

// Only create LoRa object when needed to save memory
LoRaHandler* lora = nullptr;

// MQTT config
// const char* WIFI_SSID   = "YourSSID";
// const char* WIFI_PASS   = "YourPassword";
// const char* MQTT_BROKER = "test.mosquitto.org";  
// const uint16_t MQTT_PORT = 1883;
// MQTTManager mqtt(WIFI_SSID, WIFI_PASS, MQTT_BROKER, MQTT_PORT);

// Buffer HR dari BLE ke LoRa - use smaller data types
static volatile bool hrAvailable = false;
static volatile uint16_t hrBuffer = 0; // Changed from int to uint16_t (heart rate max ~300)

// Timing constants to reduce unnecessary operations
static const uint32_t MAIN_LOOP_DELAY = 100;
static const uint32_t BLE_CHECK_INTERVAL = 1000;    // Check BLE every 1 second
static const uint32_t LORA_CHECK_INTERVAL = 50;     // Check LoRa every 50ms
static const uint32_t STATUS_REPORT_INTERVAL = 60000; // Status report every 1 minute

// Timing variables
static uint32_t lastBleCheck = 0;
static uint32_t lastLoraCheck = 0;
static uint32_t lastStatusReport = 0;

// Setup
void setup() {
    Serial.begin(115200);
    
    // Initialize only what we need based on mode
    if (isTransmitter) {
        Serial.println(F("[Main] TX Mode - Initializing BLE + LoRa"));
        
        // Initialize BLE
        ble.begin();
        ble.setHRAvailableFlag((bool*)&hrAvailable, (int*)&hrBuffer);
        
        // Create LoRa object only for TX mode
        lora = new LoRaHandler(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY,
                              LORA_SCK, LORA_MISO, LORA_MOSI);
        
        if (!lora->begin(923.0)) {
            Serial.println(F("[Main][LoRa] Gagal inisialisasi SX1262 (TX)"));
            while (true) {
                delay(1000); // Increased delay to save power
            }
        }
        
        Serial.println(F("[Main][LoRa] TX siap..."));
        
    } else {
        Serial.println(F("[Main] RX Mode - Initializing LoRa only"));
        
        // Create LoRa object only for RX mode  
        lora = new LoRaHandler(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY,
                              LORA_SCK, LORA_MISO, LORA_MOSI);
        
        if (!lora->begin(923.0)) {
            Serial.println(F("[Main][LoRa] Gagal inisialisasi SX1262 (RX)"));
            while (true) {
                delay(1000); // Increased delay to save power
            }
        }
        
        Serial.println(F("[Main][LoRa] RX Mode siap, menunggu pesan..."));
        Serial.println(F("[Main] Device ready untuk menerima data LoRa"));
        Serial.println(F("[Main] Frekuensi: 923.0 MHz"));
        
        // MQTT initialization would go here when needed
        // mqtt.begin();
    }
    
    // Initialize timing
    lastBleCheck = millis();
    lastLoraCheck = millis();
    lastStatusReport = millis();
    
    // Print startup summary
    Serial.println(F("[Main] Smartazone Device Ready"));
    if (isTransmitter) {
        Serial.println(F("Mode: TRANSMITTER (BLE → LoRa)"));
        Serial.println(F("- BLE: Mencari Heart Rate Monitor"));
        Serial.println(F("- LoRa: Siap mengirim data HR"));
    } else {
        Serial.println(F("Mode: RECEIVER (LoRa → Serial/MQTT)"));
        Serial.println(F("- LoRa: Siap menerima data"));
        Serial.println(F("- Serial: Monitoring aktif"));
    }
    Serial.println(F("Frekuensi LoRa: 923.0 MHz"));
}

// Loop
void loop() {
    uint32_t currentTime = millis();
    
    // Periodic status report for both modes
    if (currentTime - lastStatusReport >= STATUS_REPORT_INTERVAL) {
        printSystemStatus();
        lastStatusReport = currentTime;
    }
    
    if (isTransmitter) {
        // TX Mode - Optimized timing
        
        // BLE operations (less frequent to save power)
        if (currentTime - lastBleCheck >= BLE_CHECK_INTERVAL) {
            handleBLEOperations();
            lastBleCheck = currentTime;
        }
        
        // LoRa transmission (check more frequently for HR data)
        if (currentTime - lastLoraCheck >= LORA_CHECK_INTERVAL) {
            handleLoRaTransmission();
            lastLoraCheck = currentTime;
        }
        
    } else {
        // RX Mode - Check LoRa reception frequently
        if (currentTime - lastLoraCheck >= LORA_CHECK_INTERVAL) {
            handleLoRaReception();
            lastLoraCheck = currentTime;
        }
    }
    
    delay(MAIN_LOOP_DELAY);
}

// Separate function for BLE operations to reduce main loop complexity
void handleBLEOperations() {
    static bool serviceConnected = false;
    static bool notifyEnabled = false;
    
    // BLE reconnect handler
    ble.tryReconnect();
    
    // Connect to service + notify (only once)
    if (ble.isConnected()) {
        if (!serviceConnected) {
            // Use string instead of BLEUUID object
            serviceConnected = ble.connectToService(BLEUUID(HEART_RATE_SERVICE_UUID));
            if (serviceConnected) {
                Serial.println(F("[BLE] Heart Rate Service connected"));
            }
        }
        if (serviceConnected && !notifyEnabled) {
            notifyEnabled = ble.enableNotify(BLEUUID(HEART_RATE_SERVICE_UUID), 
                                           BLEUUID(HEART_RATE_CHAR_UUID));
            if (notifyEnabled) {
                Serial.println(F("[BLE] Heart Rate notifications enabled"));
            }
        }
    } else {
        // Reset flags when disconnected to retry connection
        serviceConnected = false;
        notifyEnabled = false;
    }
}

// Separate function for LoRa transmission
void handleLoRaTransmission() {
    // Send HR data if available
    if (hrAvailable && lora) {
        uint16_t hr = hrBuffer;
        hrAvailable = false;
        
        // Use String reserve to prevent memory fragmentation
        String msg;
        msg.reserve(10); // Reserve space for "HR:xxx"
        msg = F("HR:");
        msg += hr;
        
        Serial.print(F("[TX] Heart Rate: "));
        Serial.println(msg);
        lora->sendMessage(msg);
    }
}

// Separate function for LoRa reception
void handleLoRaReception() {
    if (!lora) return;
    
    static uint32_t messageCount = 0;
    String msg;
    int rssi;
    float snr;
    
    if (lora->receiveMessage(msg, rssi, snr)) {
        messageCount++;
        
        // Use F() macro to save RAM
        Serial.print(F("[RX][LoRa] Pesan #"));
        Serial.print(messageCount);
        Serial.print(F(": "));
        Serial.print(msg);
        Serial.print(F(" | RSSI:"));
        Serial.print(rssi);
        Serial.print(F(" dBm | SNR:"));
        Serial.print(snr);
        Serial.println(F(" dB"));
        
        // MQTT publish would go here
        // mqtt.publish("lora/uplink", msg);
    }
}

// System status monitoring function
void printSystemStatus() {
    uint32_t uptime = millis() / 1000; // Convert to seconds
    uint32_t hours = uptime / 3600;
    uint32_t minutes = (uptime % 3600) / 60;
    uint32_t seconds = uptime % 60;
    
    Serial.printf("Uptime: %02lu:%02lu:%02lu\n", hours, minutes, seconds);
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    
    if (isTransmitter) {
        Serial.println(F("Mode: TRANSMITTER"));
        Serial.printf("BLE Status: %s\n", ble.isConnected() ? "CONNECTED" : "DISCONNECTED");
        if (ble.isConnected()) {
            Serial.printf("Last HR: %d bpm\n", ble.getLastHeartRate());
        }
    } else {
        Serial.println(F("Mode: RECEIVER"));
        Serial.println(F("Status: LISTENING untuk pesan LoRa"));
    }
    
    if (lora) {
        Serial.println(F("LoRa: READY"));
    } else {
        Serial.println(F("LoRa: ERROR"));
    }
    
}
