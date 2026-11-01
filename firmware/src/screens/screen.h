#pragma once
#include <Arduino.h>

#include "../gfx.h"

// Cada página de la interfaz. Se cambia de una a otra deslizando el dedo
// o tocando los extremos de la barra superior.
class Screen {
 public:
  virtual ~Screen() {}
  virtual void update(float dt) {}
  virtual void draw(Gfx& g) = 0;
  virtual void onTap(int x, int y) {}
  virtual uint16_t frameMs() const { return 40; }  // cada cuánto se redibuja
};

Screen* makePetScreen();
Screen* makeDetailScreen();
Screen* makeTokensScreen();
Screen* makeSettingsScreen();
