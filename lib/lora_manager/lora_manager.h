#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include <data.h>

#ifdef DEVICE_MODE_BASE
struct receivedPacket {
    DeviceData device_data;
    float snr;
    int rssi;
    bool isNew;
};
#endif

class LoRaHandler
{
public:
    LoRaHandler(int nss, int dio1, int rst, int busy, int sck, int miso, int mosi);
    bool begin(float frequency);

    void sendMessage(const String &message);
    bool receiveMessage(String &message, int &rssi, float &snr);
    void transmit(const uint8_t* data, size_t len);
    #ifdef DEVICE_MODE_BASE
    bool receive();
    receivedPacket getNewPacket(){
        receivedPacket temp = packet_data;
        packet_data.isNew = false;
        return temp;
    }
    #endif


private:
    int _nss, _dio1, _rst, _busy;
    int _sck, _miso, _mosi;
    #ifdef DEVICE_MODE_BASE
    receivedPacket packet_data;
    #endif

    SPIClass spi;
    Module module;
    SX1262 radio;
};
