#include "lora_manager.h"

LoRaHandler::LoRaHandler(int nss, int dio1, int rst, int busy, int sck, int miso, int mosi)
    : _nss(nss), _dio1(dio1), _rst(rst), _busy(busy),
      _sck(sck), _miso(miso), _mosi(mosi),
      spi(FSPI),
      module(_nss, _dio1, _rst, _busy, spi),
      radio(&module) {}

bool LoRaHandler::begin(float frequency)
{
    Serial.println(F("[LoRa] Init SX1262..."));

    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(10);
    digitalWrite(_rst, HIGH);
    delay(10);

    spi.begin(_sck, _miso, _mosi, _nss);

    int state = radio.begin(frequency, 125.0, 7, 5, 0x34, 14, 8, 0.0f, false);
    if (state != RADIOLIB_ERR_NONE)
    {
        Serial.print(F("[LoRa] init failed: "));
        Serial.println(state);
        return false;
    }

    Serial.print(F("[LoRa] OK freq "));
    Serial.print(frequency);
    Serial.println(F(" MHz"));
    return true;
}

void LoRaHandler::sendMessage(const String &message)
{
    String msg = message; // RadioLib transmit butuh non-const
    int state = radio.transmit(msg);
    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println(F("[LoRa] Pesan terkirim"));
    }
    else
    {
        Serial.print(F("[LoRa] TX error: "));
        Serial.println(state);
    }
}

bool LoRaHandler::receiveMessage(String &message, int &rssi, float &snr)
{
    String tmp;
    int state = radio.receive(tmp);
    if (state == RADIOLIB_ERR_NONE)
    {
        message = tmp;
        rssi = radio.getRSSI();
        snr = radio.getSNR();
        return true;
    }
    return false;
}
