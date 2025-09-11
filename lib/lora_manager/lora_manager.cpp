#include "lora_manager.h"

LoRaHandler::LoRaHandler(int nss, int dio1, int rst, int busy, int sck, int miso, int mosi)
: _nss(nss), _dio1(dio1), _rst(rst), _busy(busy), _sck(sck), _miso(miso), _mosi(mosi) {
    spi = new SPIClass(FSPI);  // ESP32-S3 ada FSPI/HSPI
    radio = nullptr;
}

bool LoRaHandler::begin(float frequency) {
    Serial.println(F("[LoRa] Memulai inisialisasi SX1262..."));
    
    // Reset chip manual
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(10);
    digitalWrite(_rst, HIGH);
    delay(10);
    
    Serial.println(F("[LoRa] Reset chip selesai"));

    spi->begin(_sck, _miso, _mosi, _nss);
    Serial.println(F("[LoRa] SPI interface initialized"));
    
    radio = new SX1262(new Module(_nss, _dio1, _rst, _busy, *spi));

    int state = radio->begin(frequency, 125.0, 7, 5, 0x34, 14, 8, 0.0f, false);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[LoRa] LoRa init gagal, code: "));
        Serial.println(state);
        return false;
    }

    Serial.print(F("[LoRa] Init sukses! Frekuensi: "));
    Serial.print(frequency);
    Serial.println(F(" MHz"));
    Serial.println(F("[LoRa] Bandwidth: 125.0 kHz, SF: 7, CR: 4/5"));
    return true;
}

void LoRaHandler::sendMessage(String message) {
    int state = radio->transmit(message);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[LoRa] Pesan terkirim");
    } else {
        Serial.print("[LoRa] Gagal kirim, code: ");
        Serial.println(state);
    }
}

bool LoRaHandler::receiveMessage(String &message, int &rssi, float &snr) {
    String str;
    int state = radio->receive(str);
    if (state == RADIOLIB_ERR_NONE) {
        message = str;
        rssi = radio->getRSSI();
        snr = radio->getSNR();
        return true;
    }
    return false;
}
