#include "effects.h"

#include "../ui.h"
#include "pet.h"

void Effects::spawn(FxType type, float x, float y, float vx, float vy, float life, float size) {
  for (Particle& p : p_) {
    if (p.alive) continue;
    p = {true, type, x, y, vx, vy, 0, life, size};
    return;
  }
}

void Effects::update(float dt) {
  for (Particle& p : p_) {
    if (!p.alive) continue;
    p.age += dt;
    if (p.age >= p.life) {
      p.alive = false;
      continue;
    }
    switch (p.type) {
      case FxType::Heart: p.vy *= 1 - dt * 0.8f; break;
      case FxType::Zzz: p.vx = 8 * sinf(p.age * 3); break;
      case FxType::Sweat: p.vy += 140 * dt; break;
      case FxType::Steam: p.size += 6 * dt; break;
    }
    p.x += p.vx * dt;
    p.y += p.vy * dt;
  }
}

static void drawHeart(Gfx& g, int x, int y, int s, uint16_t c) {
  // Corazón anguloso, a juego con el estilo low poly
  g.fillTriangle(x - s, y - s / 2, x, y - s / 2, x - s / 2, y - s, c);
  g.fillTriangle(x, y - s / 2, x + s, y - s / 2, x + s / 2, y - s, c);
  g.fillTriangle(x - s, y - s / 2, x + s, y - s / 2, x, y + s, c);
}

void Effects::draw(Gfx& g) {
  static const uint16_t heart = rgb(235, 70, 110);
  static const uint16_t sweat = rgb(90, 170, 240);
  static const uint16_t steam = rgb(205, 208, 218);

  for (const Particle& p : p_) {
    if (!p.alive || p.y < SCENE_TOP) continue;
    const int x = (int)p.x, y = (int)p.y, s = (int)p.size;
    switch (p.type) {
      case FxType::Heart: drawHeart(g, x, y, s, heart); break;
      case FxType::Zzz: g.text(s > 6 ? "Z" : "z", x, y, MC_DATUM, col::muted, s > 6 ? 3 : 2); break;
      case FxType::Sweat:
        g.fillTriangle(x - s, y, x + s, y, x, y - s * 2 - 2, sweat);
        g.fillTriangle(x - s, y, x + s, y, x, y + s, sweat);
        break;
      case FxType::Steam:
        // Hexágono gris
        g.fillTriangle(x - s, y, x + s, y, x - s / 2, y - s, steam);
        g.fillTriangle(x - s / 2, y - s, x + s, y, x + s / 2, y - s, steam);
        g.fillTriangle(x - s, y, x + s, y, x - s / 2, y + s, steam);
        g.fillTriangle(x - s / 2, y + s, x + s, y, x + s / 2, y + s, steam);
        break;
    }
  }
}
