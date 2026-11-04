#include "lain.h"

#include <Arduino.h>

#include "../ui.h"
#include "lain_art2.h"

namespace {
// La imagen mide 207x185: se centra y se apoya justo encima del panel de datos.
const int X0 = (240 - LAIN_BASE_W) / 2;
const int Y0 = 56;

float rnd(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }
}  // namespace

LainPet::LainPet() {}

void LainPet::eligeCara() {
  if (mimo_ > 0) { cara_ = Feliz; return; }
  switch (mood_) {
    case Mood::Sleeping: cara_ = Cerrada; break;
    case Mood::Stressed: cara_ = Agobiada; break;
    case Mood::Busy: cara_ = Concentrada; break;
    default: cara_ = parpadeo_ > 0 ? Cerrada : Normal; break;
  }
}

void LainPet::update(const PetInput& in, float dt) {
  t_ += dt;
  mood_ = moodFrom(in);
  const bool durmiendo = mood_ == Mood::Sleeping;

  // respiración: la imagen sube un píxel y baja, muy despacio
  respiracion_ = sinf(t_ * (durmiendo ? 1.5f : 2.2f)) > 0.35f ? 1 : 0;

  parpadeo_ = max(0.0f, parpadeo_ - dt);
  if ((proximoParpadeo_ -= dt) <= 0) {
    parpadeo_ = 0.13f;
    proximoParpadeo_ = rnd(2.6f, 6.4f);
  }
  mimo_ = max(0.0f, mimo_ - dt);
  eligeCara();

  // partículas: Z al dormir, gotas al ir cargada, corazones al acariciarla
  if (durmiendo && (zzz_ -= dt) <= 0) {
    zzz_ = 1.9f;
    fx_.spawn(FxType::Zzz, X0 + 150, Y0 + 34, 6, -12, 2.6f, rnd(4, 9));
  }
  if (mood_ >= Mood::Busy && (sudor_ -= dt) <= 0) {
    sudor_ = mood_ == Mood::Stressed ? 0.7f : 2.4f;
    const float lado = rnd(0, 1) < 0.5f ? -1.0f : 1.0f;
    fx_.spawn(FxType::Sweat, X0 + 103 + lado * 44, Y0 + 76, lado * 26, -34, 0.9f, 3);
  }
  fx_.update(dt);
}

void LainPet::draw(Gfx& g) {
  const int dy = respiracion_;

  g.blit(X0, Y0 - dy, LAIN_BASE_W, LAIN_BASE_H, LAIN_BASE, true);
  if (cara_ != Normal) {
    const uint16_t* parche = LAIN_CARA_CERRADA;
    int px = LAIN_CARA_CERRADA_X, py = LAIN_CARA_CERRADA_Y;
    int pw = LAIN_CARA_CERRADA_W, ph = LAIN_CARA_CERRADA_H;
    switch (cara_) {
      case Feliz:
        parche = LAIN_CARA_FELIZ; px = LAIN_CARA_FELIZ_X; py = LAIN_CARA_FELIZ_Y;
        pw = LAIN_CARA_FELIZ_W; ph = LAIN_CARA_FELIZ_H;
        break;
      case Agobiada:
        parche = LAIN_CARA_AGOBIADA; px = LAIN_CARA_AGOBIADA_X; py = LAIN_CARA_AGOBIADA_Y;
        pw = LAIN_CARA_AGOBIADA_W; ph = LAIN_CARA_AGOBIADA_H;
        break;
      case Concentrada:
        parche = LAIN_CARA_CONCENTRADA; px = LAIN_CARA_CONCENTRADA_X; py = LAIN_CARA_CONCENTRADA_Y;
        pw = LAIN_CARA_CONCENTRADA_W; ph = LAIN_CARA_CONCENTRADA_H;
        break;
      default: break;
    }
    g.blit(X0 + px, Y0 + py - dy, pw, ph, parche, false);
  }
  fx_.draw(g);
}

void LainPet::onTap(int x, int y) {
  const bool dentro = x > X0 + 20 && x < X0 + LAIN_BASE_W - 20 && y > Y0 && y < Y0 + LAIN_BASE_H;
  if (dentro) {
    mimo_ = 2.6f;
    for (int i = 0; i < 3; i++)
      fx_.spawn(FxType::Heart, X0 + rnd(70, 140), Y0 + rnd(30, 70), rnd(-12, 12), rnd(-40, -26), 1.6f, rnd(4, 7));
  }
}
