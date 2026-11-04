#pragma once
#include <TFT_eSPI.h>

// Un búfer de pantalla completo (320x240 a 16 bits = 150 KB) no cabe en un
// solo bloque de RAM del ESP32, así que la pantalla se dibuja por franjas
// horizontales: se dibuja todo en una franja en memoria, se envía a la
// pantalla y se repite con la siguiente. Así no hay parpadeo.
//
// Gfx recibe coordenadas de pantalla y les resta la fila superior de la
// franja actual; lo que cae fuera lo recorta el propio sprite.
class Gfx {
 public:
  explicit Gfx(TFT_eSprite& sprite) : s(sprite) {}

  TFT_eSprite& s;
  int top = 0;     // fila de pantalla donde empieza la franja actual
  int height = 0;  // alto de la franja

  bool rowVisible(int y) const { return y >= top && y < top + height; }
  bool visible(int y0, int y1) const { return y1 >= top && y0 < top + height; }

  void fillRect(int x, int y, int w, int h, uint16_t c) { s.fillRect(x, y - top, w, h, c); }
  void hline(int x, int y, int w, uint16_t c) {
    if (rowVisible(y) && w > 0) s.drawFastHLine(x, y - top, w, c);
  }
  void vline(int x, int y, int h, uint16_t c) { s.drawFastVLine(x, y - top, h, c); }
  void drawRect(int x, int y, int w, int h, uint16_t c) { s.drawRect(x, y - top, w, h, c); }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    s.fillSmoothRoundRect(x, y - top, w, h, r, c);
  }
  void fillCircle(float x, float y, float r, uint16_t c) {
    if (visible(y - r - 1, y + r + 1)) s.drawSpot(x, y - top, r, c);
  }
  void fillEllipse(int x, int y, int rx, int ry, uint16_t c) {
    if (visible(y - ry, y + ry)) s.fillEllipse(x, y - top, rx, ry, c);
  }
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
    s.fillTriangle(x0, y0 - top, x1, y1 - top, x2, y2 - top, c);
  }
  // Línea gruesa con bordes suavizados (mezcla con lo que ya hay dibujado).
  void line(float x0, float y0, float x1, float y1, float w, uint16_t c) {
    if (visible(min(y0, y1) - w, max(y0, y1) + w))
      s.drawWideLine(x0, y0 - top, x1, y1 - top, w, c);
  }

  // Pinta un píxel mezclándolo con el que ya hay (alpha de 0 a 1).
  void blendPixel(int x, int y, uint16_t c, float alpha) {
    if (!rowVisible(y) || x < 0 || x >= s.width() || alpha <= 0.0f) return;
    if (alpha >= 1.0f) {
      s.drawPixel(x, y - top, c);
      return;
    }
    uint16_t bg = s.readPixel(x, y - top);
    s.drawPixel(x, y - top, s.alphaBlend((uint8_t)(alpha * 255), c, bg));
  }

  // Oscurece lo ya dibujado en una elipse (para sombras suaves).
  void shadeEllipse(int cx, int cy, int rx, int ry, float amount) {
    for (int dy = -ry; dy <= ry; dy++) {
      int y = cy + dy;
      if (!rowVisible(y)) continue;
      float k = 1.0f - (float)(dy * dy) / (float)(ry * ry);
      int hw = (int)(rx * sqrtf(max(0.0f, k)));
      for (int x = cx - hw; x <= cx + hw; x++) {
        if (x < 0 || x >= s.width()) continue;
        uint16_t bg = s.readPixel(x, y - top);
        s.drawPixel(x, y - top, s.alphaBlend((uint8_t)(amount * 255), TFT_BLACK, bg));
      }
    }
  }

  // Texto de consola (fuente de 6x8 píxeles multiplicada por size), con fondo
  // transparente. datum: TL_DATUM, MC_DATUM, MR_DATUM...
  void text(const char* str, int x, int y, uint8_t datum, uint16_t color, uint8_t size) {
    int h = size * 8;
    if (!visible(y - h, y + h)) return;
    s.setTextDatum(datum);
    s.setTextColor(color);
    s.setTextSize(size);
    s.drawString(str, x, y - top, 1);
  }

  // Pega una capa de pixel art guardada en flash (RGB565). Si clave es true,
  // el color 0xF81F se deja transparente. Escribe directo en el búfer de la
  // franja, como el motor 3D, y recorta contra los bordes.
  void blit(int x0, int y0, int w, int h, const uint16_t* src, bool clave) {
    if (y0 + h <= top || y0 >= top + height) return;
    uint16_t* buf = (uint16_t*)s.getPointer();
    const int sw = s.width();
    for (int j = 0; j < h; j++) {
      const int y = y0 + j;
      if (y < top || y >= top + height) continue;
      const uint16_t* fila = src + (long)j * w;
      uint16_t* dst = buf + (y - top) * sw;
      for (int i = 0; i < w; i++) {
        const int x = x0 + i;
        if (x < 0 || x >= sw) continue;
        const uint16_t c = pgm_read_word(fila + i);
        if (clave && c == 0xF81F) continue;
        dst[x] = (uint16_t)((c >> 8) | (c << 8));  // el sprite guarda los bytes al revés
      }
    }
  }
};
