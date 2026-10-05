/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox

   ------------------------------------------------------------------
   TFT_eSPI compile-time pin/driver configuration for this build.
   ------------------------------------------------------------------
   TFT_eSPI auto-includes this file from the sketch folder (see the
   "Sketch_with_tft_setup" example in the library) because it does
   `#if __has_include(<tft_setup.h>)` before falling back to its own
   User_Setup_Select.h. This keeps the whole pin configuration local
   to this project instead of editing files inside the library.

   Why TFT_eSPI instead of Adafruit_GFX/Adafruit_ST7789: on this exact
   GMT130/ST7789 240x240 panel (no CS pad), Adafruit_ST7789's software
   (bit-banged) SPI path compiled and ran without errors but produced
   no image at all on the physical panel (only the backlight turned
   on). Swapping to TFT_eSPI - which drives the panel over the ESP32's
   real HSPI hardware peripheral instead of bit-banging GPIOs - fixed
   it immediately on the same wiring, same pins, same breadboard. Kept
   for the record in case the Adafruit path is revisited later.
   ________________________________________ */

#define USER_SETUP_LOADED

#define ST7789_DRIVER
#define TFT_RGB_ORDER TFT_RGB   // flip to TFT_BGR here if colours ever look swapped

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// Physical pins - keep in sync with the table in
// docs/custom-build/WIRING.md and with the pin numbers referenced in
// display_compat.h's comments.
#define TFT_MOSI  21   // SDA on the panel
#define TFT_SCLK  22   // SCK
#define TFT_DC    15   // DC
#define TFT_RST    2   // RES

// The panel has no real CS pad (tied to GND on its own PCB), but
// TFT_eSPI's driver still wants a valid GPIO number internally. This
// pin is NOT wired to anything on the panel - it's a harmless unused
// GPIO purely to satisfy the library (same workaround used
// successfully in this project's sibling repo, claudio-bot).
#define TFT_CS    12

// NOTE: TFT_BL / TFT_BACKLIGHT_ON are intentionally NOT defined here.
// Backlight brightness (GPIO13) is driven separately via the ESP32
// ledc PWM API in display_compat.h, so TFT_eSPI must not also try to
// drive that same pin with a plain digitalWrite() inside init().

// Use the HSPI peripheral, not VSPI - VSPI (GPIO18/19/23) is already
// used by the two NRF24L01 radios on their own hardware SPI bus, and
// this panel's pins (21/22) must stay on a fully separate peripheral
// so radio and display SPI transactions never interfere with one
// another.
#define USE_HSPI_PORT

#define LOAD_GLCD   1
#define LOAD_FONT2  1
#define LOAD_FONT4  1
#define LOAD_FONT6  1
#define LOAD_FONT7  1
#define LOAD_FONT8  1
#define LOAD_GFXFF  1
#define SMOOTH_FONT 1

#define SPI_FREQUENCY       20000000
#define SPI_READ_FREQUENCY  20000000
