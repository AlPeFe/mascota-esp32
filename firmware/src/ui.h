#pragma once
#include <Arduino.h>

#include "gfx.h"

// Colores y piezas de interfaz compartidas por todas las pantallas.
// Estilo: fondo blanco, tinta casi negra y texto de consola.

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

namespace col {
constexpr uint16_t bg = rgb(255, 255, 255);
constexpr uint16_t line = rgb(205, 208, 216);
constexpr uint16_t track = rgb(226, 228, 234);
constexpr uint16_t text = rgb(28, 30, 42);
constexpr uint16_t muted = rgb(120, 124, 140);
constexpr uint16_t accent = rgb(0, 140, 120);
constexpr uint16_t warn = rgb(215, 140, 10);
constexpr uint16_t bad = rgb(225, 50, 50);
constexpr uint16_t net = rgb(40, 110, 220);
}  // namespace col

uint16_t lerp565(uint16_t a, uint16_t b, float t);
uint16_t levelColor(float pct);  // tinta / ámbar / rojo según el %

void fmtRate(char* out, size_t n, float kbps);      // "512 KB/s", "3.4 MB/s"
void fmtRateTiny(char* out, size_t n, float kbps);  // "512K", "3.4M"

void drawTopBar(Gfx& g, int page, int pages);
void drawBar(Gfx& g, int x, int y, int w, int h, float pct, uint16_t color);  // barra en bloques
void drawCard(Gfx& g, int x, int y, int w, int h);
void drawSpark(Gfx& g, int x, int y, int w, int h, const float* hist, float maxV, uint16_t color);
void drawArrow(Gfx& g, int x, int y, bool up, uint16_t color);
