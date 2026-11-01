// Página de ajustes: mascota, brillo, orientación, giro y colores.
#include "../app.h"
#include "../config.h"
#include "../pets/pet.h"
#include "../settings.h"
#include "../stats.h"
#include "../ui.h"
#include "screen.h"

namespace {

const int ROW_COUNT = 5;
const int BTN = 28;

class SettingsScreen : public Screen {
 public:
  uint16_t frameMs() const override { return 1000; }

  void draw(Gfx& g) override {
    const int w = SCREEN_W;
    char buf[32];

    row(g, 0, "Mascota");
    button(g, w - 128, 0, "<");
    g.text(PETS[settings.pet].name, w - 72, midY(0), MC_DATUM, col::text, 1);
    button(g, w - 40, 0, ">");

    row(g, 1, "Brillo");
    button(g, w - 128, 1, "-");
    drawBar(g, w - 96, midY(1) - 4, 54, 8, settings.brightness * 100.0f / 255, col::text);
    button(g, w - 40, 1, "+");

    row(g, 2, "Orientacion");
    g.text(settings.vertical ? "[vertical]" : "[horizontal]", w - 12, midY(2), MR_DATUM, col::text, 1);

    row(g, 3, "Girar 180");
    check(g, 3, settings.flip);

    row(g, 4, "Invertir");
    check(g, 4, settings.invert);

    snprintf(buf, sizeof(buf), "v" FIRMWARE_VERSION " - pc %s", statsConnected() ? "conectado" : "sin datos");
    g.text(buf, w / 2, (rowY(ROW_COUNT - 1) + rowH() + SCREEN_H) / 2, MC_DATUM, col::muted, 1);

    drawTopBar(g, appPage(), PAGE_COUNT);
  }

  void onTap(int x, int y) override {
    int r = -1;
    for (int i = 0; i < ROW_COUNT; i++)
      if (y >= rowY(i) && y < rowY(i) + rowH()) r = i;
    if (r < 0) return;

    const int w = SCREEN_W;
    const bool left = x >= w - 134 && x < w - 94;
    const bool right = x >= w - 46;
    switch (r) {
      case 0:
        if (left) settings.pet = (settings.pet + PET_COUNT - 1) % PET_COUNT;
        else if (right) settings.pet = (settings.pet + 1) % PET_COUNT;
        else return;
        appSetPet(settings.pet);
        break;
      case 1:
        if (left) settings.brightness = max(15, settings.brightness - 30);
        else if (right) settings.brightness = min(255, settings.brightness + 30);
        else return;
        break;
      case 2:
        if (x < w - 100) return;
        settings.vertical = !settings.vertical;
        break;
      case 3:
        if (x < w - 80) return;
        settings.flip = !settings.flip;
        break;
      case 4:
        if (x < w - 80) return;
        settings.invert = !settings.invert;
        break;
    }
    settingsSave();
    appApplyDisplay();
  }

 private:
  int rowH() const { return layout.portrait ? 44 : 34; }
  int rowY(int i) const { return (layout.portrait ? 28 : 24) + i * (rowH() + 4); }
  int midY(int i) const { return rowY(i) + rowH() / 2; }

  void row(Gfx& g, int i, const char* label) {
    g.text(label, 8, midY(i), ML_DATUM, col::text, 1);
    g.hline(8, rowY(i) + rowH() + 1, SCREEN_W - 16, col::line);
  }

  void button(Gfx& g, int x, int i, const char* label) {
    g.drawRect(x, rowY(i) + (rowH() - BTN) / 2, BTN, BTN, col::muted);
    g.text(label, x + BTN / 2, midY(i), MC_DATUM, col::text, 2);
  }

  void check(Gfx& g, int i, bool on) { g.text(on ? "[x]" : "[ ]", SCREEN_W - 12, midY(i), MR_DATUM, col::text, 2); }
};

}  // namespace

Screen* makeSettingsScreen() { return new SettingsScreen(); }
