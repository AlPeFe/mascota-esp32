#pragma once
#include "../gfx.h"

// Partículas sencillas que cualquier mascota puede usar.
// Data: la luz que escupe el cacharro de Lain cuando lo mira.
enum class FxType : uint8_t { Heart, Zzz, Sweat, Steam, Smoke, Data };

struct Particle {
  bool alive;
  FxType type;
  float x, y, vx, vy;
  float age, life, size;
};

class Effects {
 public:
  void spawn(FxType type, float x, float y, float vx, float vy, float life, float size);
  void update(float dt);
  void draw(Gfx& g);

 private:
  static const int MAX = 40;
  Particle p_[MAX] = {};
};
