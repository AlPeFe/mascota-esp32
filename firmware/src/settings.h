#pragma once
#include <Arduino.h>

// Ajustes guardados en la memoria flash (se conservan al desenchufar).
struct Settings {
  uint8_t pet = 0;          // índice en la lista de mascotas
  uint8_t brightness = 200;  // 10..255
  bool invert = false;      // invertir colores de la pantalla
  bool vertical = true;     // vertical (240x320) u horizontal (320x240)
  bool flip = false;        // girar 180º
};

extern Settings settings;

void settingsLoad();
void settingsSave();
