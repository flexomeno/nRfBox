/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox

   ------------------------------------------------------------------
   Display compatibility shim
   ------------------------------------------------------------------
   The original nRFBox firmware was written against a monochrome
   128x64 I2C OLED (SSD1306) using the U8g2 library. This build
   replaces that screen with a 240x240 SPI IPS panel (GMT130 /
   ST7789 driver, labelled GND VCC SCK SDA RES DC BLK - no CS pad).

   Rather than rewriting every pixel-exact screen in ism.cpp,
   bluetooth.cpp, wifi.cpp, setting.cpp and nRFBox.ino, this class
   exposes the *exact same* method names used everywhere in the
   codebase (clearBuffer, sendBuffer, drawStr, print, setCursor,
   setFont, drawXBMP, drawVLine/HLine/Line/Pixel, drawBox/Frame/
   RFrame, setDrawColor, begin, setContrast, setBitmapMode,
   getUTF8Width...) and forwards them to:
     - Adafruit_ST7789 for raw graphics primitives, and
     - U8g2_for_Adafruit_GFX for text/fonts (reuses the original
       u8g2 font tables, so every u8g2_font_xxx reference keeps
       working unmodified).

   The original 128x64 logical canvas is kept untouched and is
   rendered centered on the physical 240x240 panel, so every menu,
   icon and graph keeps its original layout/position.

   IMPORTANT (wiring): this GMT130/ST7789 panel has no CS pin - its
   chip-select is tied low on the board, so it is ALWAYS selected.
   For that reason it MUST NOT share the MOSI/SCK lines used by the
   two nRF24 modules (hardware VSPI on GPIO18/19/23), or every SPI
   transaction meant for the radios would also be latched by the
   screen (and vice-versa), corrupting both. This shim therefore
   drives the display over a separate, bit-banged SPI bus on its
   own dedicated GPIOs (see TFT_* pins below) that are completely
   disconnected from the radios' bus.
   ________________________________________ */

#ifndef DISPLAY_COMPAT_H
#define DISPLAY_COMPAT_H

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <U8g2_for_Adafruit_GFX.h>

// ---------------------------------------------------------------
// Physical panel (GMT130, 1.3" IPS, ST7789 driver, 240x240, no CS)
// ---------------------------------------------------------------
#define TFT_PHYS_WIDTH   240
#define TFT_PHYS_HEIGHT  240

// Dedicated / bit-banged SPI pins for the display. Keep these OFF
// the hardware VSPI bus (GPIO18=SCK, GPIO19=MISO, GPIO23=MOSI) that
// the nRF24 modules use, since the panel has no CS line of its own.
#define TFT_SCLK_PIN   22   // SCK  (free since the OLED's I2C SCL is no longer used)
#define TFT_MOSI_PIN   21   // SDA  (free since the OLED's I2C SDA is no longer used)
#define TFT_RST_PIN     2   // RES
#define TFT_DC_PIN     15   // DC   (free now that NRF module "C" was removed)
#define TFT_BLK_PIN    13   // BLK  (backlight, PWM brightness control)
#define TFT_CS_PIN     -1   // no CS pad on this module - tied low on the board

// Rotation 0..3 - flip this on the bench if the image appears
// upside down / mirrored for your particular panel batch.
#define TFT_ROTATION    0

// ---------------------------------------------------------------
// Logical UI canvas: kept at the ORIGINAL 128x64 size on purpose,
// so every hand-tuned pixel coordinate in the rest of the firmware
// (menus, icons, graphs...) keeps working exactly as before. It is
// rendered centered on the bigger 240x240 physical panel.
// ---------------------------------------------------------------
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 128
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 64
#endif

#define TFT_OFFSET_X  ((TFT_PHYS_WIDTH  - SCREEN_WIDTH)  / 2)
#define TFT_OFFSET_Y  ((TFT_PHYS_HEIGHT - SCREEN_HEIGHT) / 2)

#define TFT_COLOR_ON   ST77XX_WHITE
#define TFT_COLOR_OFF  ST77XX_BLACK

class U8g2Compat : public Print {
public:
  U8g2Compat()
    : tft(TFT_CS_PIN, TFT_DC_PIN, TFT_MOSI_PIN, TFT_SCLK_PIN, TFT_RST_PIN) {}

  // --- lifecycle -------------------------------------------------
  void begin() {
    if (_began) return;
    _began = true;

    pinMode(TFT_BLK_PIN, OUTPUT);
    ledcAttach(TFT_BLK_PIN, 5000, 8);   // ESP32 core 3.x ledc API
    ledcWrite(TFT_BLK_PIN, 255);

    tft.init(TFT_PHYS_WIDTH, TFT_PHYS_HEIGHT);
    tft.setSPISpeed(20000000);
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(TFT_COLOR_OFF);

    u8f.begin(tft);
    u8f.setFontMode(1);       // transparent text background by default
    u8f.setFontDirection(0);
    setDrawColor(1);
  }

  // --- u8g2-style buffer API (ST7789 draws immediately, so these
  //     only need to clear / no-op) --------------------------------
  void clearBuffer() { tft.fillScreen(TFT_COLOR_OFF); }
  void sendBuffer()  { /* nothing to flush, draws are immediate */ }

  // --- fonts / text (forwarded to U8g2_for_Adafruit_GFX) ----------
  void setFont(const uint8_t *font) { u8f.setFont(font); }

  void setCursor(int16_t x, int16_t y) {
    u8f.setCursor(x + TFT_OFFSET_X, y + TFT_OFFSET_Y);
  }

  size_t write(uint8_t c) override { return u8f.write(c); }
  size_t write(const uint8_t *buf, size_t size) override { return u8f.write(buf, size); }

  uint16_t drawStr(int16_t x, int16_t y, const char *s) {
    return u8f.drawStr(x + TFT_OFFSET_X, y + TFT_OFFSET_Y, s);
  }

  uint16_t getUTF8Width(const char *s) { return u8f.getUTF8Width(s); }

  // --- color / bitmap mode -----------------------------------------
  void setDrawColor(uint8_t c) {
    _color = c ? 1 : 0;
    u8f.setForegroundColor(_color ? TFT_COLOR_ON : TFT_COLOR_OFF);
    u8f.setBackgroundColor(_color ? TFT_COLOR_OFF : TFT_COLOR_ON);
  }

  // u8g2 semantics: 0 = solid (background pixels are also painted),
  // 1 = transparent (only "on" pixels are painted).
  void setBitmapMode(uint8_t isTransparent) { _bitmapTransparent = (isTransparent != 0); }

  void drawXBMP(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *bitmap) {
    const uint16_t fg = _color ? TFT_COLOR_ON : TFT_COLOR_OFF;
    const uint16_t bg = _color ? TFT_COLOR_OFF : TFT_COLOR_ON;
    const int16_t bytesPerRow = (w + 7) / 8;

    tft.startWrite();
    for (int16_t row = 0; row < h; row++) {
      for (int16_t col = 0; col < w; col++) {
        uint8_t byte = pgm_read_byte(bitmap + row * bytesPerRow + (col / 8));
        bool on = byte & (1 << (col % 8));
        if (on) {
          tft.writePixel(x + col + TFT_OFFSET_X, y + row + TFT_OFFSET_Y, fg);
        } else if (!_bitmapTransparent) {
          tft.writePixel(x + col + TFT_OFFSET_X, y + row + TFT_OFFSET_Y, bg);
        }
      }
    }
    tft.endWrite();
  }

  // --- basic primitives ---------------------------------------------
  void drawPixel(int16_t x, int16_t y) {
    tft.drawPixel(x + TFT_OFFSET_X, y + TFT_OFFSET_Y, _color ? TFT_COLOR_ON : TFT_COLOR_OFF);
  }

  void drawHLine(int16_t x, int16_t y, int16_t w) {
    tft.drawFastHLine(x + TFT_OFFSET_X, y + TFT_OFFSET_Y, w, _color ? TFT_COLOR_ON : TFT_COLOR_OFF);
  }

  void drawVLine(int16_t x, int16_t y, int16_t h) {
    tft.drawFastVLine(x + TFT_OFFSET_X, y + TFT_OFFSET_Y, h, _color ? TFT_COLOR_ON : TFT_COLOR_OFF);
  }

  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    tft.drawLine(x0 + TFT_OFFSET_X, y0 + TFT_OFFSET_Y,
                 x1 + TFT_OFFSET_X, y1 + TFT_OFFSET_Y,
                 _color ? TFT_COLOR_ON : TFT_COLOR_OFF);
  }

  void drawBox(int16_t x, int16_t y, int16_t w, int16_t h) {
    tft.fillRect(x + TFT_OFFSET_X, y + TFT_OFFSET_Y, w, h, _color ? TFT_COLOR_ON : TFT_COLOR_OFF);
  }

  void drawFrame(int16_t x, int16_t y, int16_t w, int16_t h) {
    tft.drawRect(x + TFT_OFFSET_X, y + TFT_OFFSET_Y, w, h, _color ? TFT_COLOR_ON : TFT_COLOR_OFF);
  }

  void drawRFrame(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) {
    tft.drawRoundRect(x + TFT_OFFSET_X, y + TFT_OFFSET_Y, w, h, r, _color ? TFT_COLOR_ON : TFT_COLOR_OFF);
  }

  // --- "contrast" -> backlight brightness (0-255) --------------------
  void setContrast(uint8_t v) {
    if (!_began) return;
    ledcWrite(TFT_BLK_PIN, v);
  }

private:
  Adafruit_ST7789 tft;
  U8G2_FOR_ADAFRUIT_GFX u8f;
  uint8_t _color = 1;
  bool _bitmapTransparent = false; // u8g2 default bitmap mode is "solid"
  bool _began = false;
};

#endif // DISPLAY_COMPAT_H
