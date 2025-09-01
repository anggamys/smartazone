#ifndef CONFIG_H
#define CONFIG_H

// Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define SDA_PIN 18
#define SCL_PIN 17

// BLE Configuration
#define BLE_DEVICE_NAME "ESP32_BLE"
#define BLE_DEVICE_MAC "F8:FD:E8:E8:84:37"
#define BLE_SCAN_TIME 10
#define CONNECTION_TIMEOUT 10000

// Application Configuration
#define SERIAL_BAUD_RATE 115200
#define MAIN_LOOP_DELAY 100
#define STATUS_UPDATE_INTERVAL 2000
#define SENSOR_READ_INTERVAL 5000

#endif // CONFIG_H
