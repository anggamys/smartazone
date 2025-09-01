#include "display_manager.h"

DisplayManager::DisplayManager() 
    : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
      status(DisplayStatus::UNINITIALIZED),
      lastUpdateTime(0) {}

bool DisplayManager::initialize() {
    Wire.begin(SDA_PIN, SCL_PIN);
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println(F("[DISPLAY] Initialization failed"));
        status = DisplayStatus::ERROR;
        return false;
    }
    
    status = DisplayStatus::READY;
    clear();
    Serial.println(F("[DISPLAY] Initialized successfully"));
    return true;
}

void DisplayManager::clear() {
    if (status != DisplayStatus::READY) return;
    
    display.clearDisplay();
    update();
}

void DisplayManager::update() {
    if (status != DisplayStatus::READY) return;
    
    display.display();
    lastUpdateTime = millis();
}

void DisplayManager::setDefaultTextSettings() {
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(true);
}

bool DisplayManager::validateCoordinates(int x, int y) const {
    return (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT);
}

bool DisplayManager::printText(const String &text, int x, int y, 
                              int textSize, bool clearBefore) {
    if (status != DisplayStatus::READY) return false;
    if (!validateCoordinates(x, y)) return false;
    
    if (clearBefore) {
        display.clearDisplay();
    }
    
    setDefaultTextSettings();
    display.setTextSize(textSize);
    display.setCursor(x, y);
    display.println(text);
    update();
    
    return true;
}

bool DisplayManager::printTwoLines(const String &line1, const String &line2, 
                                  int spacing) {
    if (status != DisplayStatus::READY) return false;
    
    display.clearDisplay();
    setDefaultTextSettings();
    display.setTextSize(1);
    
    display.setCursor(0, 10);
    display.println(line1);
    display.setCursor(0, 10 + spacing);
    display.println(line2);
    
    update();
    return true;
}

bool DisplayManager::printCenteredText(const String &text, int y, int textSize) {
    if (status != DisplayStatus::READY) return false;
    
    // Calculate text width for centering
    int16_t x1, y1;
    uint16_t w, h;
    display.setTextSize(textSize);
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    
    int x = (SCREEN_WIDTH - w) / 2;
    return printText(text, x, y, textSize, true);
}

bool DisplayManager::drawBitmap(const uint8_t *bitmap, int x, int y, int w, int h) {
    if (status != DisplayStatus::READY) return false;
    if (!validateCoordinates(x, y)) return false;
    
    display.clearDisplay();
    display.drawBitmap(x, y, bitmap, w, h, SSD1306_WHITE);
    update();
    
    return true;
}

void DisplayManager::drawProgressBar(int x, int y, int width, int height, 
                                   int progress, bool showPercentage) {
    if (status != DisplayStatus::READY) return;
    
    // Clamp progress to 0-100
    progress = constrain(progress, 0, 100);
    
    // Draw outer rectangle
    display.drawRect(x, y, width, height, SSD1306_WHITE);
    
    // Draw filled progress
    int fillWidth = map(progress, 0, 100, 0, width - 2);
    display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
    
    // Show percentage if requested
    if (showPercentage) {
        String progressText = String(progress) + "%";
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(progressText, 0, 0, &x1, &y1, &w, &h);
        
        int textX = x + (width - w) / 2;
        int textY = y + height + 5;
        display.setCursor(textX, textY);
        display.println(progressText);
    }
    
    update();
}

void DisplayManager::drawConnectionStatus(bool isConnected) {
    if (status != DisplayStatus::READY) return;
    
    // Draw connection indicator in top-right corner
    int x = SCREEN_WIDTH - 8;
    int y = 4;
    
    if (isConnected) {
        display.fillCircle(x, y, 3, SSD1306_WHITE);
    } else {
        display.drawCircle(x, y, 3, SSD1306_WHITE);
    }
    
    update();
}
