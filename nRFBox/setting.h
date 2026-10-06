/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/nrfbox
   ________________________________________ */
   
#ifndef setting_H
#define setting_H

#include <BLEDevice.h>
#include "display_compat.h"
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <RF24.h>
#include <vector>
#include <string>
#include <SD.h>
#include <Update.h>
#include <SPI.h>

void neopixelSetup();
void neopixelLoop();

void setNeoPixelColour(const std::string& colour);
void flash(int numberOfFlashes, const std::vector<std::string>& colors, const std::string& finalColour);

extern U8g2Compat u8g2;
extern Adafruit_NeoPixel pixels;

extern bool neoPixelActive;
extern uint8_t oledBrightness;

// TEMPORAL: solo 2 modulos nRF24 fisicos cableados por ahora (A y B),
// mientras se terminan las pruebas. El tercero (RadioC) se re-agregara
// mas adelante - ver docs/custom-build/WIRING.md seccion 9.
extern RF24 RadioA;
extern RF24 RadioB;

void configureNrf(RF24 &radio);

void setRadiosNeutralState();

void setupRadioA();
void setupRadioB();

void initAllRadios();

void Str(uint8_t x, uint8_t y, const uint8_t* asciiArray, size_t len);
void CenteredStr(uint8_t screenWidth, uint8_t y, const uint8_t* asciiArray, size_t len, const uint8_t* font);
void utils();
void conf();

namespace Setting {
  void settingSetup();
  void settingLoop();
}

#endif
