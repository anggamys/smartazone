#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

enum class DisplayStatus {
    UNINITIALIZED,
    READY,
    ERROR
};

class DisplayManager {
private:
    Adafruit_SSD1306 display;
    DisplayStatus status;
    unsigned long lastUpdateTime;
    
    // Internal helper methods
    void setDefaultTextSettings();
    bool validateCoordinates(int x, int y) const;

public:
    DisplayManager();
    ~DisplayManager() = default;

    // Core functionality
    bool initialize();
    void clear();
    void update();
    
    // Text display methods
    bool printText(const String &text, int x = 0, int y = 0, 
                   int textSize = 1, bool clearBefore = true);
    bool printTwoLines(const String &line1, const String &line2, 
                       int spacing = 20);
    bool printCenteredText(const String &text, int y = 32, int textSize = 1);
    
    // Graphics methods
    bool drawBitmap(const uint8_t *bitmap, int x, int y, int w, int h);
    void drawProgressBar(int x, int y, int width, int height, 
                        int progress, bool showPercentage = true);
    void drawConnectionStatus(bool isConnected);
    
    // Status methods
    DisplayStatus getStatus() const { return status; }
    bool isReady() const { return status == DisplayStatus::READY; }
    unsigned long getLastUpdateTime() const { return lastUpdateTime; }
};

#endif // DISPLAY_MANAGER_H
