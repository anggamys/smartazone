#include "Arduino.h"
#ifndef DATA_H
#define DATA_H
struct Location
{
    float lattitude, longitude;
};
union SensorData
{
    uint8_t value;
    Location location;
};
enum class Topic : uint8_t
{
    HEART_RATE=1,
    SPO2=2,
    STRESS=3,
    GPS=4
};

struct DeviceData
{
    SensorData sensor;
    uint8_t device_id;
    Topic topic;
};

struct GPSData
{
    float lattitude;
    float longitude;
    bool isNew;
};
#endif
