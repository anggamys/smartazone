#include "lora_manager.h"

LoRaHandler::LoRaHandler(int nss, int dio1, int rst, int busy, int sck, int miso, int mosi)
: _nss(nss), _dio1(dio1), _rst(rst), _busy(busy), _sck(sck), _miso(miso), _mosi(mosi) {
    spi = new SPIClass(FSPI);
    radio = nullptr;
}

LoRaHandler::~LoRaHandler() {
    if (radio) {
        delete radio;
        radio = nullptr;
    }
    if (spi) {
        // don't delete SPIClass (keeps system SPI stable)
        spi = nullptr;
    }
}

bool LoRaHandler::begin(float frequency) {
    Serial.println(F("[LoRa] Initializing SX1262..."));

    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(10);
    digitalWrite(_rst, HIGH);
    delay(10);

    spi->begin(_sck, _miso, _mosi, _nss);
    Serial.println(F("[LoRa] SPI initialized"));

    // Create module and radio
    radio = new SX1262(new Module(_nss, _dio1, _rst, _busy, *spi));

    int state = radio->begin(frequency, 125.0, 7, 5, 0x34, 14, 8, 0.0f, false);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(F("[LoRa] init failed code: "));
        Serial.println(state);
        return false;
    }

    Serial.print(F("[LoRa] OK freq: "));
    Serial.print(frequency);
    Serial.println(F(" MHz"));
    return true;
}

void LoRaHandler::sendMessage(const String &message) {
    if (!radio) {
        Serial.println(F("[LoRa] sendMessage: radio not initialized"));
        return;
    }
    // RadioLib::transmit expects a non-const String&, so make a local copy.
    String msg = message;
    int state = radio->transmit(msg);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("[LoRa] Pesan terkirim"));
    } else {
        Serial.print(F("[LoRa] transmit error code: "));
        Serial.println(state);
    }
}

bool LoRaHandler::receiveMessage(String &message, int &rssi, float &snr) {
    if (!radio) return false;

    String tmp;
    int state = radio->receive(tmp); // this will block until receive or timeout configured in RadioLib internals
    if (state == RADIOLIB_ERR_NONE) {
        message = tmp;
        rssi = radio->getRSSI();
        snr = radio->getSNR();
        return true;
    }
    // If RX timeout, just return false (no message)
    return false;
}
