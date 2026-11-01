#pragma once
#include <Arduino.h>

#define FIRMWARE_VERSION "2.1"

// --- Táctil XPT2046 (va en su propio bus SPI) ---
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK 25

// Calibración: valores en bruto del XPT2046 en los bordes de la pantalla.
// Si al tocar el punto no coincide con el dedo, ajusta estos cuatro números.
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 240
#define TOUCH_Y_MAX 3800

// --- Otros pines de la placa ---
#define PIN_BACKLIGHT 21
#define PIN_LED_R 4  // LED RGB trasero (activo a nivel bajo)
#define PIN_LED_G 16
#define PIN_LED_B 17

// --- Pantalla ---
#define STRIP_H 80  // alto de cada franja que se dibuja en memoria
#define TOPBAR_H 20

// Medidas que dependen de la orientación (se calculan en main.cpp).
struct Layout {
  int w = 240, h = 320;
  bool portrait = true;
  int floorY = 236;                // donde apoya la mascota
  int petX = 120;      // centro de la mascota y su escritorio
  float petScale = 1;  // tamaño de la mascota
};
extern Layout layout;
#define SCREEN_W layout.w
#define SCREEN_H layout.h

// --- Comunicación con el PC ---
#define SERIAL_BAUD 115200
#define DATA_TIMEOUT_MS 5000      // sin datos durante este tiempo = desconectado
#define DIM_AFTER_MS 60000        // sin datos durante este tiempo = bajar brillo
#define HISTORY_LEN 60            // muestras guardadas para las gráficas

#ifndef DEFAULT_INVERT
#define DEFAULT_INVERT 0
#endif
