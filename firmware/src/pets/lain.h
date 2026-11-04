#pragma once
#include "effects.h"
#include "pet.h"

// Lain en pixel art: una imagen de fondo (la del diseno aprobado) y parches de
// cara encima para las expresiones. Se anima con el parpadeo, la respiracion de
// un pixel y las particulas.
class LainPet : public Pet {
 public:
  LainPet();

  void update(const PetInput& in, float dt) override;
  void draw(Gfx& g) override;
  void onTap(int x, int y) override;
  Mood mood() const override { return mood_; }

 private:
  enum Cara : uint8_t { Normal, Feliz, Cerrada, Agobiada, Concentrada };

  void eligeCara();

  Mood mood_ = Mood::Sleeping;
  float t_ = 0;
  int respiracion_ = 0;     // 0 o 1: la imagen sube un pixel al respirar
  float parpadeo_ = 0;      // > 0 mientras tiene los ojos cerrados
  float proximoParpadeo_ = 3;
  float mimo_ = 0;          // > 0 tras acariciarla
  float zzz_ = 0, sudor_ = 0;
  Cara cara_ = Normal;
  Effects fx_;
};
