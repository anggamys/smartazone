#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>

struct SensorData {
    // Health metrics
    uint16_t heartRate;        // beats per minute
    uint32_t stepCount;        // total steps
    uint16_t calories;         // calories burned
    float temperature;         // body temperature in Celsius
    uint8_t batteryLevel;      // battery percentage
    
    // Data validity flags
    bool heartRateValid;
    bool stepCountValid;
    bool caloriesValid;
    bool temperatureValid;
    bool batteryValid;
    
    // Timestamp
    unsigned long timestamp;
    
    // Constructor
    SensorData() { clear(); }
    
    // Utility methods
    void clear() {
        heartRate = stepCount = calories = batteryLevel = 0;
        temperature = 0.0;
        heartRateValid = stepCountValid = caloriesValid = 
        temperatureValid = batteryValid = false;
        timestamp = millis();
    }
    
    bool hasValidData() const {
        return heartRateValid || stepCountValid || caloriesValid || 
               temperatureValid || batteryValid;
    }
    
    String toString() const {
        String result = "Sensor Data:\n";
        if (heartRateValid) result += "HR: " + String(heartRate) + " bpm\n";
        if (stepCountValid) result += "Steps: " + String(stepCount) + "\n";
        if (caloriesValid) result += "Calories: " + String(calories) + "\n";
        if (temperatureValid) result += "Temp: " + String(temperature, 1) + "°C\n";
        if (batteryValid) result += "Battery: " + String(batteryLevel) + "%\n";
        return result;
    }
};

#endif // SENSOR_DATA_H
