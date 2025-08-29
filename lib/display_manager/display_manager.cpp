#include "display_manager.h"

DisplayManager::DisplayManager(int sda, int scl)
  : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
    sdaPin(sda), sclPin(scl) {}

bool DisplayManager::begin() {
  Wire.begin(sdaPin, sclPin);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 init failed"));
    return false;
  }
  clear();
  return true;
}

void DisplayManager::clear() {
  display.clearDisplay();
  display.display();
}

void DisplayManager::printText(const String &text, int x, int y, int textSize, bool clearBefore) {
  if (clearBefore) clear();
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.println(text);
  display.display();
}

void DisplayManager::printTwoLine(const String &line1, const String &line2) {
  clear();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println(line1);
  display.setCursor(0, 30);
  display.println(line2);
  display.display();
}

void DisplayManager::drawBitmap(const uint8_t *bitmap, int w, int h) {
  clear();
  display.drawBitmap(0, 0, bitmap, w, h, SSD1306_WHITE);
  display.display();
}

void DisplayManager::drawProgressBar(int x, int y, int width, int height, int progress) {
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  int fill = (progress * (width - 2)) / 100;
  display.fillRect(x + 1, y + 1, fill, height - 2, SSD1306_WHITE);
  display.display();
}
