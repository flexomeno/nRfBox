/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */

#ifndef CONFIG_H
#define CONFIG_H

// Logical UI canvas (kept at the original size on purpose - see
// display_compat.h for how this is mapped onto the new 240x240 panel).
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Push Buttons-specific Pins
#define BUTTON_UP_PIN       26 
#define BUTTON_SELECT_PIN   33
#define BUTTON_DOWN_PIN     32 
#define BTN_PIN_RIGHT       27
#define BTN_PIN_LEFT        25

// SD Card Slot-specific Pins (optional - only used by Setting > Update Firmware)
#define SD_CS_PIN 5
#define FIRMWARE_FILE "/firmware.bin"

// nRF24-specific Pins (TEMPORAL: solo 2 modulos cableados por ahora -
// A y B - mientras se terminan las pruebas. El tercero (RadioC) se
// re-agregara mas adelante; ver docs/custom-build/WIRING.md seccion 9)
#define NRF_CE_PIN_A    5   
#define NRF_CSN_PIN_A   17 
#define NRF_CE_PIN_B    16  
#define NRF_CSN_PIN_B   4  

// Common dependencies
#include "setting.h"
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <Preferences.h>
#include <vector>
#include <string>
#include <SD.h>
#include <Update.h>

// Display: GMT130 / ST7789 240x240 IPS SPI panel, driven through the
// U8g2Compat shim so every existing u8g2.* call keeps working.
// (The actual objects are defined once in setting.cpp - see setting.h -
// to avoid "duplicate symbol" link errors, since this header is
// included from several separate .cpp translation units.)
#include "display_compat.h"

// BLE-specific dependencies
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// nRF24-specific dependencies
#include <nRF24L01.h>
#include <RF24.h>

// WiFi-specific dependencies
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_system.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <string>

// ESP-specific configurations
#include <esp_bt.h>
#include <esp_wifi.h>

// External declarations
extern U8g2Compat u8g2;
extern Adafruit_NeoPixel pixels;

// BLE-related namespaces
namespace BleJammer {
  void blejammerSetup();
  void blejammerLoop();
}

namespace BleScan {
  void blescanSetup();
  void blescanLoop();
}

namespace SourApple {
  void sourappleSetup();
  void sourappleLoop();
}

namespace Spoofer {
  void spooferSetup();
  void spooferLoop();
}

// nRF24-related namespaces
namespace Analyzer {
  void analyzerSetup();
  void analyzerLoop();
}

namespace ProtoKill {
  void blackoutSetup();
  void blackoutLoop();
}

namespace Scanner {
  void scannerSetup();
  void scannerLoop();
}

namespace Jammer {
  void jammerSetup();
  void jammerLoop();
}

// WiFi-related namespaces
namespace WifiScan {
  void wifiscanSetup();
  void wifiscanLoop();
}

namespace Deauther {
  void deautherSetup();
  void deautherLoop();
}

#endif // CONFIG_H
