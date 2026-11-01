// Página principal: la mascota sobre blanco y las métricas como texto de consola.
#include "../app.h"
#include "../config.h"
#include "../pets/pet.h"
#include "../stats.h"
#include "../ui.h"
#include "screen.h"

namespace {

class PetScreen : public Screen {
 public:
  void update(float dt) override { t_ += dt; }

  void draw(Gfx& g) override {
    appPet()->draw(g);
    drawMood(g);
    if (layout.portrait) drawConsolePortrait(g);
    else drawConsoleLandscape(g);
    drawTopBar(g, appPage(), PAGE_COUNT);
  }

  void onTap(int x, int y) override { appPet()->onTap(x, y); }

 private:
  void drawMood(Gfx& g) {
    char buf[20];
    const bool cursor = fmodf(t_, 1.0f) < 0.5f;
    snprintf(buf, sizeof(buf), "> %s%s", moodName(appPet()->mood()), cursor ? "_" : "");
    g.text(buf, 8, 24, TL_DATUM, col::muted, 1);
  }

  // "CPU  41%" en tamaño grande. Devuelve si hay dato que mostrar.
  bool metric(Gfx& g, int x, int y, const char* label, float pct) {
    const bool has = statsConnected() && pct >= 0;
    char buf[12];
    if (has) snprintf(buf, sizeof(buf), "%s %3d%%", label, (int)roundf(pct));
    else snprintf(buf, sizeof(buf), "%s   --", label);
    g.text(buf, x, y, TL_DATUM, has ? levelColor(pct) : col::muted, 2);
    return has;
  }

  void drawConsolePortrait(Gfx& g) {
    const int barX = 112, barW = SCREEN_W - barX - 30;
    const float values[3] = {stats.cpu, stats.ram, stats.gpu};
    const char* labels[3] = {"CPU", "RAM", "GPU"};
    int y = 246;
    for (int i = 0; i < 3; i++, y += 18) {
      bool has = metric(g, 8, y, labels[i], values[i]);
      drawBar(g, barX, y + 2, barW, 11, has ? values[i] : 0, levelColor(values[i]));
    }
    if (statsConnected() && stats.gpuTemp >= 0) {
      char buf[8];
      snprintf(buf, sizeof(buf), "%dC", (int)stats.gpuTemp);
      g.text(buf, SCREEN_W - 8, 246 + 36 + 4, TR_DATUM, stats.gpuTemp >= 80 ? col::bad : col::muted, 1);
    }

    if (!statsConnected()) {
      g.text("NET   --", 8, y, TL_DATUM, col::muted, 2);
      return;
    }
    char rate[8];
    g.text("NET", 8, y, TL_DATUM, col::text, 2);
    drawArrow(g, 56, y + 7, false, col::net);
    fmtRateTiny(rate, sizeof(rate), stats.netDown);
    g.text(rate, 64, y, TL_DATUM, col::text, 2);
    const int upX = 64 + 6 * 12 + 8;
    drawArrow(g, upX, y + 7, true, col::accent);
    fmtRateTiny(rate, sizeof(rate), stats.netUp);
    g.text(rate, upX + 8, y, TL_DATUM, col::text, 2);
  }

  void drawConsoleLandscape(Gfx& g) {
    const float values[3] = {stats.cpu, stats.ram, stats.gpu};
    const char* labels[3] = {"CPU", "RAM", "GPU"};
    int y = 46;
    for (int i = 0; i < 3; i++, y += 42) {
      bool has = metric(g, 8, y, labels[i], values[i]);
      drawBar(g, 8, y + 19, 94, 8, has ? values[i] : 0, levelColor(values[i]));
    }
    if (statsConnected() && stats.gpuTemp >= 0) {
      char buf[8];
      snprintf(buf, sizeof(buf), "%dC", (int)stats.gpuTemp);
      g.text(buf, 108, 46 + 84 + 19, TL_DATUM, stats.gpuTemp >= 80 ? col::bad : col::muted, 1);
    }

    const bool on = statsConnected();
    char rate[8];
    drawArrow(g, 12, y + 7, false, col::net);
    fmtRateTiny(rate, sizeof(rate), stats.netDown);
    g.text(on ? rate : "--", 22, y, TL_DATUM, on ? col::text : col::muted, 2);
    drawArrow(g, 12, y + 25, true, col::accent);
    fmtRateTiny(rate, sizeof(rate), stats.netUp);
    g.text(on ? rate : "--", 22, y + 18, TL_DATUM, on ? col::text : col::muted, 2);
  }

  float t_ = 0;
};

}  // namespace

Screen* makePetScreen() { return new PetScreen(); }
