#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

class DisplayManager {
  private:
    Adafruit_SSD1306 display;
    int sdaPin;
    int sclPin;

  public:
    DisplayManager(int sda, int scl);

    bool begin();
    void clear();
    void printText(const String &text, int x=0, int y=0, int textSize=1, bool clearBefore=true);
    void printTwoLine(const String &line1, const String &line2);
    void drawBitmap(const uint8_t *bitmap, int w, int h);
    void drawProgressBar(int x, int y, int width, int height, int progress);
};

#endif
