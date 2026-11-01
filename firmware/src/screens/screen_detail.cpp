// Página de recursos en detalle: seis recuadros en dos columnas.
#include "../app.h"
#include "../config.h"
#include "../stats.h"
#include "../ui.h"
#include "screen.h"

namespace {

class DetailScreen : public Screen {
 public:
  uint16_t frameMs() const override { return 500; }

  void draw(Gfx& g) override {
    on_ = statsConnected();
    narrow_ = layout.portrait;
    w_ = narrow_ ? 116 : 154;
    h_ = narrow_ ? 96 : 68;
    const int cx[2] = {narrow_ ? 3 : 4, narrow_ ? 121 : 162};
    const int ry[3] = {narrow_ ? 23 : 24, narrow_ ? 122 : 96, narrow_ ? 221 : 168};

    cpu(g, cx[0], ry[0]);
    cores(g, cx[1], ry[0]);
    ram(g, cx[0], ry[1]);
    gpu(g, cx[1], ry[1]);
    disk(g, cx[0], ry[2]);
    net(g, cx[1], ry[2]);
    drawTopBar(g, appPage(), PAGE_COUNT);
  }

 private:
  // Marco común: recuadro, título y valor grande. En vertical los recuadros son
  // estrechos y altos, así que el valor va en su propia fila.
  bool card(Gfx& g, int x, int y, const char* title, float pct) {
    if (!g.visible(y, y + h_)) return false;
    drawCard(g, x, y, w_, h_);
    g.text(title, x + 8, y + 7, TL_DATUM, col::muted, 1);
    char buf[8];
    bool has = on_ && pct >= 0;
    if (has) snprintf(buf, sizeof(buf), "%d%%", (int)roundf(pct));
    if (pct > -2) {
      const char* value = has ? buf : "--";
      uint16_t color = has ? levelColor(pct) : col::muted;
      if (narrow_) g.text(value, x + 8, y + 20, TL_DATUM, color, 3);
      else g.text(value, x + w_ - 8, y + 5, TR_DATUM, color, 3);
    }
    return true;
  }

  int contentY(int y) const { return y + (narrow_ ? 50 : 34); }
  int barY(int y) const { return y + h_ - 13; }

  void cpu(Gfx& g, int x, int y) {
    if (!card(g, x, y, "CPU", stats.cpu)) return;
    int top = contentY(y);
    drawSpark(g, x + 8, top, w_ - 16, y + h_ - 6 - top, stats.cpuHist, 100, col::text);
  }

  void cores(Gfx& g, int x, int y) {
    char title[16];
    snprintf(title, sizeof(title), "NUCLEOS %d", stats.coreCount);
    if (!card(g, x, y, title, -2)) return;
    int n = stats.coreCount;
    if (!on_ || n == 0) return;
    float bw = (float)(w_ - 16) / n;
    int by = y + 22, barH = h_ - 28;
    for (int i = 0; i < n; i++) {
      int bx = x + 8 + (int)(i * bw);
      int w = max(1, (int)bw - 1);
      int fh = barH * stats.cores[i] / 100;
      g.fillRect(bx, by, w, barH, col::track);
      g.fillRect(bx, by + barH - fh, w, fh, levelColor(stats.cores[i]));
    }
  }

  void ram(Gfx& g, int x, int y) {
    if (!card(g, x, y, "RAM", stats.ram)) return;
    if (on_) {
      char buf[24];
      snprintf(buf, sizeof(buf), "%.1f / %.1f GB", stats.ramUsed, stats.ramTotal);
      g.text(buf, x + 8, contentY(y), TL_DATUM, col::text, 1);
    }
    drawBar(g, x + 8, barY(y), w_ - 16, 7, on_ ? stats.ram : 0, levelColor(stats.ram));
  }

  void gpu(Gfx& g, int x, int y) {
    if (!card(g, x, y, stats.gpuName[0] ? stats.gpuName : "GPU", stats.gpu)) return;
    if (!on_ || stats.vram < 0) return;
    char buf[24];
    snprintf(buf, sizeof(buf), stats.vramUsed < 10 ? "VRAM %.1f/%.0f GB" : "VRAM %.0f/%.0f GB",
             stats.vramUsed, stats.vramTotal);
    g.text(buf, x + 8, contentY(y), TL_DATUM, col::text, 1);
    if (stats.gpuTemp >= 0) {
      snprintf(buf, sizeof(buf), "%dC", (int)stats.gpuTemp);
      uint16_t color = stats.gpuTemp >= 80 ? col::bad : col::muted;
      g.text(buf, x + w_ - 8, narrow_ ? y + 28 : contentY(y), TR_DATUM, color, 1);
    }
    drawBar(g, x + 8, barY(y), w_ - 16, 7, stats.vram, levelColor(stats.vram));
  }

  void disk(Gfx& g, int x, int y) {
    if (!card(g, x, y, "DISCO", stats.disk)) return;
    if (!on_) return;
    char buf[16];
    int top = contentY(y);
    int writeX = narrow_ ? x + 8 : x + w_ / 2 + 2;
    int writeY = narrow_ ? top + 12 : top;
    g.text("L", x + 8, top, TL_DATUM, col::muted, 1);
    fmtRate(buf, sizeof(buf), stats.diskRead);
    g.text(buf, x + 18, top, TL_DATUM, col::text, 1);
    g.text("E", writeX, writeY, TL_DATUM, col::muted, 1);
    fmtRate(buf, sizeof(buf), stats.diskWrite);
    g.text(buf, writeX + 10, writeY, TL_DATUM, col::text, 1);
    drawBar(g, x + 8, barY(y), w_ - 16, 7, stats.disk, levelColor(stats.disk));
  }

  void net(Gfx& g, int x, int y) {
    if (!card(g, x, y, "RED", -2)) return;
    if (!on_) return;
    char buf[16];
    // En vertical las velocidades van bajo el título; en horizontal, a su derecha
    const int y1 = y + (narrow_ ? 28 : 11), y2 = y + (narrow_ ? 42 : 25);
    const int arrowX = narrow_ ? x + 12 : x + 70;
    drawArrow(g, arrowX, y1, false, col::net);
    fmtRate(buf, sizeof(buf), stats.netDown);
    g.text(buf, x + w_ - 8, y1, MR_DATUM, col::text, 1);
    drawArrow(g, arrowX, y2, true, col::accent);
    fmtRate(buf, sizeof(buf), stats.netUp);
    g.text(buf, x + w_ - 8, y2, MR_DATUM, col::text, 1);

    float maxV = 100;  // KB/s mínimo de escala
    for (float v : stats.netHist) maxV = max(maxV, v);
    int top = y + (narrow_ ? 54 : 38);
    drawSpark(g, x + 8, top, w_ - 16, y + h_ - 6 - top, stats.netHist, maxV, col::net);
  }

  bool on_ = false, narrow_ = true;
  int w_ = 116, h_ = 96;
};

}  // namespace

Screen* makeDetailScreen() { return new DetailScreen(); }
