#include "lora_manager.h"

LoRaHandler::LoRaHandler(int nss, int dio1, int rst, int busy, int sck, int miso, int mosi)
: _nss(nss), _dio1(dio1), _rst(rst), _busy(busy), _sck(sck), _miso(miso), _mosi(mosi) {
    spi = new SPIClass(FSPI);  // ESP32-S3 ada FSPI/HSPI
    radio = nullptr;
}

bool LoRaHandler::begin(float frequency) {
    // Reset chip manual
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(10);
    digitalWrite(_rst, HIGH);
    delay(10);

    // Init SPI (coba FSPI, kalau gagal bisa HSPI)
    spi->begin(_sck, _miso, _mosi, _nss);

    radio = new SX1262(new Module(_nss, _dio1, _rst, _busy, *spi));

    int state = radio->begin(frequency, 125.0, 7, 5, 0x34, 14);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("[LoRa] LoRa init gagal, code: ");
        Serial.println(state);
        return false;
    }
    Serial.println("[LoRa] Init sukses!");
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
