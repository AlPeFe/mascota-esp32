#pragma once
#include "../config.h"
#include "../gfx.h"

// Lo que la mascota sabe del PC.
struct PetInput {
  bool connected;
  float cpu;      // % suavizado
  float gpu;      // % (-1 si no hay datos)
  int idle;       // segundos sin tocar teclado ni ratón (-1 si no se sabe)
};

enum class Mood : uint8_t {
  Sleeping,  // sin PC, o llevas mucho rato sin tocarlo
  Relaxed,   // has parado un momento: descansa con su café
  Working,   // estás trabajando: teclea
  Busy,      // trabajas y el PC va cargado: teclea muy deprisa, concentrada
  Stressed,  // PC al límite: agobiada, suda y echa vapor
};

Mood moodFrom(const PetInput& in);
const char* moodName(Mood m);

// Zona de la pantalla donde vive la mascota.
#define SCENE_TOP 20
#define FLOOR_Y layout.floorY

// Para añadir una mascota nueva: crea una clase que herede de Pet y
// añádela a la lista PETS en pets.cpp. Aparecerá en Ajustes.
class Pet {
 public:
  virtual ~Pet() {}
  virtual void update(const PetInput& in, float dt) = 0;
  virtual void draw(Gfx& g) = 0;               // solo la mascota y sus efectos
  virtual void onTap(int x, int y) = 0;        // toque dentro de la escena
  virtual Mood mood() const = 0;
  // Para la orden !info: vértices, triángulos y piezas que usa el modelo.
  virtual void modelStats(int& verts, int& tris, int& parts, int& maxV, int& maxT, int& maxP) const {
    verts = tris = parts = maxV = maxT = maxP = 0;
  }
};

struct PetInfo {
  const char* name;
  Pet* (*create)();
};

extern const PetInfo PETS[];
extern const int PET_COUNT;
