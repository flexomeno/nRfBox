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
     - TFT_eSPI for raw graphics primitives, and
     - U8g2_for_Adafruit_GFX for text/fonts (reuses the original
       u8g2 font tables, so every u8g2_font_xxx reference keeps
       working unmodified).

   The original 128x64 logical canvas is kept untouched (every
   hand-tuned pixel coordinate in the rest of the firmware keeps
   meaning exactly what it always meant), but it is now UPSCALED
   (nearest-neighbour, uniform on both axes to avoid distorting
   icons/circles into ellipses) to fill as much of the physical
   240x240 panel as possible without stretching, then centered.
   See UI_SCALE below.

   IMPORTANT (wiring): this GMT130/ST7789 panel has no CS pin - its
   chip-select is tied low on the board, so it is ALWAYS selected.
   For that reason it MUST NOT share the MOSI/SCK lines used by the
   two nRF24 modules (hardware VSPI on GPIO18/19/23), or every SPI
   transaction meant for the radios would also be latched by the
   screen (and vice-versa), corrupting both. This shim therefore
   drives the display over the ESP32's *other* hardware SPI
   peripheral (HSPI) on its own dedicated GPIOs (see tft_setup.h),
   completely separate from the radios' VSPI bus.

   IMPORTANT (library choice): this used to be implemented on top of
   Adafruit_GFX + Adafruit_ST7789 using that library's software
   (bit-banged) SPI mode. That compiled and ran without any error on
   this exact panel/wiring, but produced NO image at all on real
   hardware - only the backlight turned on, nothing was ever drawn.
   Switching to TFT_eSPI (real HSPI hardware peripheral instead of
   bit-banged GPIOs) fixed it immediately with the *same* pins on the
   *same* wiring - so if you ever see a similar "backlight on, blank
   screen" symptom again, suspect the SPI implementation before
   suspecting the wiring. Pin/driver config lives in tft_setup.h
   (auto-included by TFT_eSPI from the sketch folder).
   ________________________________________ */

#ifndef DISPLAY_COMPAT_H
#define DISPLAY_COMPAT_H

#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include <TFT_eSPI.h>
// Adafruit_GFX is used here ONLY as a thin adapter base class (see
// TftGfxBridge below) - TFT_eSPI does its own thing and does NOT
// derive from Adafruit_GFX (it only derives from Print), but
// U8g2_for_Adafruit_GFX::begin() requires an Adafruit_GFX&. The
// bridge forwards the handful of primitives U8g2_for_Adafruit_GFX
// actually calls (drawPixel/drawFastHLine/drawFastVLine) straight
// through to the real TFT_eSPI instance, through the SAME
// scale+offset mapping used everywhere else in this file, so scaled
// u8g2 fonts "just work" for free.
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

// ---------------------------------------------------------------
// Physical panel (GMT130, 1.3" IPS, ST7789 driver, 240x240, no CS)
// ---------------------------------------------------------------
#define TFT_PHYS_WIDTH   240
#define TFT_PHYS_HEIGHT  240

// Pin numbers are defined at COMPILE TIME for TFT_eSPI, in
// tft_setup.h (auto-included by the library from the sketch folder -
// see the big comment block above). Only the backlight pin is
// handled here directly, since it's driven by our own ledc PWM call
// below rather than by TFT_eSPI itself (see tft_setup.h for why
// TFT_BL/TFT_BACKLIGHT_ON are deliberately NOT defined there).
#define TFT_BLK_PIN    13   // BLK  (backlight, PWM brightness control)

// Rotation 0..3 - flip this on the bench if the image appears
// upside down / mirrored for your particular panel batch.
#define TFT_ROTATION    0

// ---------------------------------------------------------------
// Logical UI canvas: kept at the ORIGINAL 128x64 size on purpose,
// so every hand-tuned pixel coordinate in the rest of the firmware
// (menus, icons, graphs...) keeps meaning exactly what it always
// meant. It is then upscaled (nearest-neighbour) and centered on
// the bigger 240x240 physical panel - see UI_SCALE just below.
// ---------------------------------------------------------------
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 128
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 64
#endif

// UNIFORM scale factor (same on X and Y) so icons/selection circles
// don't turn into ellipses. Based on the WIDTH ratio (240/128 =
// 1.875), since width is the tighter constraint here: at this scale
// the UI fills the panel's full 240px width and ~120 of its 240px
// height (centered, with top/bottom margins) - much bigger than the
// unscaled 128x64-in-a-240x240-box look, with zero distortion.
static constexpr float UI_SCALE_X = (float)TFT_PHYS_WIDTH / (float)SCREEN_WIDTH;
static constexpr float UI_SCALE_Y = UI_SCALE_X; // uniform on purpose

static constexpr int16_t TFT_OFFSET_X =
    (TFT_PHYS_WIDTH - (int16_t)(SCREEN_WIDTH * UI_SCALE_X)) / 2;
static constexpr int16_t TFT_OFFSET_Y =
    (TFT_PHYS_HEIGHT - (int16_t)(SCREEN_HEIGHT * UI_SCALE_Y)) / 2;

#define TFT_COLOR_ON   TFT_WHITE
#define TFT_COLOR_OFF  TFT_BLACK

// --- scale+offset mapping helpers --------------------------------
// rawX/rawY: logical -> scaled physical offset (no centering yet).
// Using floor() on BOTH the start and the end of a span (instead of
// just multiplying a fixed block size) guarantees adjacent logical
// pixels/runs produce perfectly adjacent scaled blocks with no gaps
// and no overlaps, even though UI_SCALE is not an integer.
static inline int16_t uiRawX(int16_t x) { return (int16_t)floorf(x * UI_SCALE_X); }
static inline int16_t uiRawY(int16_t y) { return (int16_t)floorf(y * UI_SCALE_Y); }
static inline int16_t uiMapX(int16_t x) { return uiRawX(x) + TFT_OFFSET_X; }
static inline int16_t uiMapY(int16_t y) { return uiRawY(y) + TFT_OFFSET_Y; }
static inline int16_t uiSpanW(int16_t x, int16_t w) { return uiRawX((int16_t)(x + w)) - uiRawX(x); }
static inline int16_t uiSpanH(int16_t y, int16_t h) { return uiRawY((int16_t)(y + h)) - uiRawY(y); }

// Minimal Adafruit_GFX-shaped adapter around a TFT_eSPI instance -
// see the include comment above for why this exists. Only the 3
// primitives U8g2_for_Adafruit_GFX actually calls on its `gfx`
// pointer are overridden; drawPixel is pure-virtual in Adafruit_GFX
// so it must be implemented even though u8f never calls it directly
// with single pixels (it uses runs via drawFastHLine/VLine for font
// glyphs, which is exactly why those two matter here). All three
// apply the SAME scale+offset mapping as everything else in this
// file, so u8g2 fonts get upscaled along with the rest of the UI.
class TftGfxBridge : public Adafruit_GFX {
public:
  TftGfxBridge(TFT_eSPI &t)
    : Adafruit_GFX(TFT_PHYS_WIDTH, TFT_PHYS_HEIGHT), _t(t) {}

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    _t.fillRect(uiMapX(x), uiMapY(y), uiSpanW(x, 1), uiSpanH(y, 1), color);
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
    _t.fillRect(uiMapX(x), uiMapY(y), uiSpanW(x, w), uiSpanH(y, 1), color);
  }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
    _t.fillRect(uiMapX(x), uiMapY(y), uiSpanW(x, 1), uiSpanH(y, h), color);
  }

private:
  TFT_eSPI &_t;
};

class U8g2Compat : public Print {
public:
  U8g2Compat() : gfxBridge(tft) {}

  // --- lifecycle -------------------------------------------------
  void begin() {
    if (_began) return;
    _began = true;

    pinMode(TFT_BLK_PIN, OUTPUT);
    ledcAttach(TFT_BLK_PIN, 5000, 8);   // ESP32 core 3.x ledc API
    ledcWrite(TFT_BLK_PIN, 255);

    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(TFT_COLOR_OFF);

    // u8f draws entirely in LOGICAL 128x64 coordinates - all the
    // scale+offset math happens inside TftGfxBridge, once, for
    // everything (text included). Don't add TFT_OFFSET_* here too,
    // or text would be mapped twice.
    u8f.begin(gfxBridge);
    u8f.setFontMode(1);       // transparent text background by default
    u8f.setFontDirection(0);
    setDrawColor(1);
  }

  // --- u8g2-style buffer API (ST7789 draws immediately, so these
  //     only need to clear / no-op) --------------------------------
  void clearBuffer() { tft.fillScreen(TFT_COLOR_OFF); }
  void sendBuffer()  { /* nothing to flush, draws are immediate */ }

  // --- fonts / text (forwarded to U8g2_for_Adafruit_GFX, logical coords) --
  void setFont(const uint8_t *font) { u8f.setFont(font); }

  void setCursor(int16_t x, int16_t y) {
    u8f.setCursor(x, y);
  }

  size_t write(uint8_t c) override { return u8f.write(c); }
  size_t write(const uint8_t *buf, size_t size) override { return u8f.write(buf, size); }

  uint16_t drawStr(int16_t x, int16_t y, const char *s) {
    return u8f.drawStr(x, y, s);
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

  // Icons (16x16 XBM bitmaps): each logical bit becomes a scaled,
  // gap-free block via uiSpanW/uiSpanH - nearest-neighbour upscale.
  void drawXBMP(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *bitmap) {
    const uint16_t fg = _color ? TFT_COLOR_ON : TFT_COLOR_OFF;
    const uint16_t bg = _color ? TFT_COLOR_OFF : TFT_COLOR_ON;
    const int16_t bytesPerRow = (w + 7) / 8;

    tft.startWrite();
    for (int16_t row = 0; row < h; row++) {
      const int16_t by = uiMapY(y + row), bh = uiSpanH(y + row, 1);
      for (int16_t col = 0; col < w; col++) {
        uint8_t byte = pgm_read_byte(bitmap + row * bytesPerRow + (col / 8));
        bool on = byte & (1 << (col % 8));
        if (on) {
          tft.fillRect(uiMapX(x + col), by, uiSpanW(x + col, 1), bh, fg);
        } else if (!_bitmapTransparent) {
          tft.fillRect(uiMapX(x + col), by, uiSpanW(x + col, 1), bh, bg);
        }
      }
    }
    tft.endWrite();
  }

  // --- basic primitives (all scaled+offset via uiMap*/uiSpan*) -------
  void drawPixel(int16_t x, int16_t y) {
    uint16_t c = _color ? TFT_COLOR_ON : TFT_COLOR_OFF;
    tft.fillRect(uiMapX(x), uiMapY(y), uiSpanW(x, 1), uiSpanH(y, 1), c);
  }

  void drawHLine(int16_t x, int16_t y, int16_t w) {
    uint16_t c = _color ? TFT_COLOR_ON : TFT_COLOR_OFF;
    tft.fillRect(uiMapX(x), uiMapY(y), uiSpanW(x, w), uiSpanH(y, 1), c);
  }

  void drawVLine(int16_t x, int16_t y, int16_t h) {
    uint16_t c = _color ? TFT_COLOR_ON : TFT_COLOR_OFF;
    tft.fillRect(uiMapX(x), uiMapY(y), uiSpanW(x, 1), uiSpanH(y, h), c);
  }

  // Diagonal lines keep a 1px-thin scaled stroke (rare in this UI,
  // not worth the complexity of a thick-diagonal-line algorithm).
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    tft.drawLine(uiMapX(x0), uiMapY(y0), uiMapX(x1), uiMapY(y1),
                 _color ? TFT_COLOR_ON : TFT_COLOR_OFF);
  }

  void drawBox(int16_t x, int16_t y, int16_t w, int16_t h) {
    tft.fillRect(uiMapX(x), uiMapY(y), uiSpanW(x, w), uiSpanH(y, h),
                 _color ? TFT_COLOR_ON : TFT_COLOR_OFF);
  }

  // Rect OUTLINE, drawn as 4 filled strips so the border thickness
  // scales up along with everything else (a plain scaled
  // tft.drawRect() would stay a hairline 1px border, which looks
  // flimsy next to the now-much-bigger icons/text).
  void drawFrame(int16_t x, int16_t y, int16_t w, int16_t h) {
    uint16_t c = _color ? TFT_COLOR_ON : TFT_COLOR_OFF;
    int16_t X = uiMapX(x), Y = uiMapY(y);
    int16_t W = uiSpanW(x, w), H = uiSpanH(y, h);
    int16_t tx = uiSpanW(x, 1), ty = uiSpanH(y, 1);
    tft.fillRect(X, Y, W, ty, c);                  // top
    tft.fillRect(X, Y + H - ty, W, ty, c);          // bottom
    tft.fillRect(X, Y, tx, H, c);                   // left
    tft.fillRect(X + W - tx, Y, tx, H, c);          // right
  }

  // Rounded rect outline (used for the menu selection highlight).
  // TFT_eSPI's drawRoundRect() only draws a 1px outline, so this
  // draws a handful of concentric outlines (as many as the scaled
  // "1 logical pixel" thickness) to approximate a thicker border
  // without reimplementing round-rect rasterization.
  void drawRFrame(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) {
    uint16_t c = _color ? TFT_COLOR_ON : TFT_COLOR_OFF;
    int16_t X = uiMapX(x), Y = uiMapY(y);
    int16_t W = uiSpanW(x, w), H = uiSpanH(y, h);
    int16_t R = (int16_t)(r * UI_SCALE_X);
    int16_t thickness = max((int16_t)1, uiSpanW(x, 1));
    for (int16_t i = 0; i < thickness; i++) {
      int16_t rr = R - i;
      if (rr < 0) rr = 0;
      tft.drawRoundRect(X + i, Y + i, W - 2 * i, H - 2 * i, rr, c);
    }
  }

  // --- "contrast" -> backlight brightness (0-255) --------------------
  void setContrast(uint8_t v) {
    if (!_began) return;
    ledcWrite(TFT_BLK_PIN, v);
  }

private:
  TFT_eSPI tft;             // must be declared before gfxBridge (init order)
  TftGfxBridge gfxBridge;
  U8G2_FOR_ADAFRUIT_GFX u8f;
  uint8_t _color = 1;
  bool _bitmapTransparent = false; // u8g2 default bitmap mode is "solid"
  bool _began = false;
};

#endif // DISPLAY_COMPAT_H
