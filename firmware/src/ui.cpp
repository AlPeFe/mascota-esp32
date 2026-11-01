#include "ui.h"

#include "config.h"
#include "stats.h"

uint16_t lerp565(uint16_t a, uint16_t b, float t) {
  if (t <= 0) return a;
  if (t >= 1) return b;
  int ar = a >> 11, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = b >> 11, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  int r = ar + (int)((br - ar) * t);
  int g = ag + (int)((bg - ag) * t);
  int bl = ab + (int)((bb - ab) * t);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

uint16_t levelColor(float pct) {
  if (pct >= 85) return col::bad;
  if (pct >= 60) return col::warn;
  return col::text;
}

void fmtRate(char* out, size_t n, float kbps) {
  if (kbps >= 1024 * 100) snprintf(out, n, "%.0f MB/s", kbps / 1024);
  else if (kbps >= 1024) snprintf(out, n, "%.1f MB/s", kbps / 1024);
  else snprintf(out, n, "%.0f KB/s", kbps);
}

void fmtRateTiny(char* out, size_t n, float kbps) {
  if (kbps >= 1024 * 100) snprintf(out, n, "%.0fM", kbps / 1024);
  else if (kbps >= 1024) snprintf(out, n, "%.1fM", kbps / 1024);
  else snprintf(out, n, "%.0fK", kbps);
}

void drawTopBar(Gfx& g, int page, int pages) {
  if (!g.visible(0, TOPBAR_H)) return;

  char buf[12];
  if (stats.hour >= 0) snprintf(buf, sizeof(buf), "%02d:%02d", stats.hour, stats.minute);
  else strcpy(buf, "--:--");
  g.text(buf, 8, 10, ML_DATUM, col::text, 1);

  snprintf(buf, sizeof(buf), "[%d/%d]", page + 1, pages);
  g.text(buf, SCREEN_W / 2, 10, MC_DATUM, col::muted, 1);

  bool on = statsConnected();
  g.text(on ? "usb ok" : "sin pc", SCREEN_W - 8, 10, MR_DATUM, on ? col::accent : col::bad, 1);
}

void drawBar(Gfx& g, int x, int y, int w, int h, float pct, uint16_t color) {
  if (!g.visible(y, y + h)) return;
  const int block = 6, gap = 2;
  const int count = (w + gap) / (block + gap);
  const int full = (int)ceilf(count * constrain(pct, 0.0f, 100.0f) / 100.0f);
  for (int i = 0; i < count; i++) g.fillRect(x + i * (block + gap), y, block, h, i < full ? color : col::track);
}

void drawCard(Gfx& g, int x, int y, int w, int h) {
  if (g.visible(y, y + h)) g.drawRect(x, y, w, h, col::line);
}

void drawSpark(Gfx& g, int x, int y, int w, int h, const float* hist, float maxV, uint16_t color) {
  if (!g.visible(y, y + h) || maxV <= 0) return;
  float step = (float)w / (HISTORY_LEN - 1);
  float px = 0, py = 0;
  for (int i = 0; i < HISTORY_LEN; i++) {
    float v = hist[(stats.histHead + i) % HISTORY_LEN];  // de la más antigua a la más nueva
    float cx = x + i * step;
    float cy = y + h - constrain(v / maxV, 0.0f, 1.0f) * h;
    g.vline((int)cx, (int)cy, y + h - (int)cy, col::track);
    if (i > 0) g.line(px, py, cx, cy, 1.4f, color);
    px = cx;
    py = cy;
  }
}

void drawArrow(Gfx& g, int x, int y, bool up, uint16_t color) {
  if (up) g.fillTriangle(x - 4, y + 3, x + 4, y + 3, x, y - 4, color);
  else g.fillTriangle(x - 4, y - 3, x + 4, y - 3, x, y + 4, color);
}
