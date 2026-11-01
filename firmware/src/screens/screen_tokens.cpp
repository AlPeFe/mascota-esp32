// Página de tokens: reservada para más adelante.
#include "../app.h"
#include "../config.h"
#include "../ui.h"
#include "screen.h"

namespace {

class TokensScreen : public Screen {
 public:
  uint16_t frameMs() const override { return 1000; }

  void draw(Gfx& g) override {
    const int cy = (TOPBAR_H + SCREEN_H) / 2;
    g.text("TOKENS", SCREEN_W / 2, cy - 30, MC_DATUM, col::text, 3);
    g.text("> proximamente_", SCREEN_W / 2, cy, MC_DATUM, col::accent, 1);
    g.text("Aqui veras el consumo de", SCREEN_W / 2, cy + 22, MC_DATUM, col::muted, 1);
    g.text("tokens que envie el PC.", SCREEN_W / 2, cy + 34, MC_DATUM, col::muted, 1);
    drawTopBar(g, appPage(), PAGE_COUNT);
  }
};

}  // namespace

Screen* makeTokensScreen() { return new TokensScreen(); }
