#pragma once
#include <Arduino.h>
#include <RadioLib.h>

class LoRaHandler
{
public:
    LoRaHandler(int nss, int dio1, int rst, int busy, int sck, int miso, int mosi);
    bool begin(float frequency);

    void sendMessage(const String &message);
    bool receiveMessage(String &message, int &rssi, float &snr);

private:
    int _nss, _dio1, _rst, _busy;
    int _sck, _miso, _mosi;

    SPIClass spi;
    Module module;
    SX1262 radio;
};
