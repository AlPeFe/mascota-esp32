#pragma once
#include "../gfx.h"

// Partículas sencillas que cualquier mascota puede usar.
enum class FxType : uint8_t { Heart, Zzz, Sweat, Steam };

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
