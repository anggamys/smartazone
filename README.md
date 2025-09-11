# Smartazone Device Firmware

This repository contains the firmware for the Smartazone device, which serves as a bridge between Bluetooth Low Energy (BLE) heart rate monitors and LoRaWAN networks. The device can operate in two modes: as a transmitter (receiving data from BLE devices and sending it over LoRa) or as a receiver (receiving data over LoRa and forwarding it to an MQTT broker).

## Features

- Connects to BLE heart rate monitors and reads heart rate data.
- Transmits heart rate data over LoRaWAN.
- Receives data over LoRaWAN and forwards it to an MQTT broker.
- Configurable device mode (transmitter or receiver).
- Status reporting via serial console.

## Usage

1. Clone the repository to your local machine.
2. Open the project in PlatformIO.
3. Configure the device mode by setting the `DEVICE_MODE_TX` macro in [src/main.cpp](src/main.cpp):
   - Set to `1` for transmitter mode (BLE to LoRa).
   - Set to `0` for receiver mode (LoRa to MQTT).
4. Build and upload the firmware to your ESP32 device.
5. Monitor the serial output for status messages and data transmission logs.
