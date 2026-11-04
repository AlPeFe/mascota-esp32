#include "critter.h"
#include "lain.h"
#include "pet.h"

// Colores: pelo, interior de las orejas, camiseta, raya, pantalón y zapatos.
// Las dos últimas parejas son la melena y la cinta (las usa la especie humana).
static const Critter::Species CAT = {
    Critter::Beast, false, true, 1.0f,
    {205, 212, 232}, {248, 196, 205}, {60, 118, 205}, {120, 205, 225}, {70, 78, 110}, {52, 58, 96},
    {0, 0, 0}, {0, 0, 0},
};
static const Critter::Species BUNNY = {
    Critter::Beast, true, false, 0.92f,
    {245, 205, 218}, {252, 170, 185}, {90, 185, 150}, {245, 235, 170}, {80, 110, 130}, {90, 66, 90},
    {0, 0, 0}, {0, 0, 0},
};

static Pet* createCat() { return new Critter(CAT); }
static Pet* createBunny() { return new Critter(BUNNY); }
static Pet* createLain() { return new LainPet(); }

// Lista de mascotas disponibles. La primera es la que sale por defecto.
const PetInfo PETS[] = {
    {"Lain", createLain},
    {"Gato", createCat},
    {"Conejo", createBunny},
};
const int PET_COUNT = sizeof(PETS) / sizeof(PETS[0]);

static const int WORKING_IDLE_S = 45;    // segundos sin actividad para dejar de teclear
static const int SLEEP_IDLE_S = 3 * 60;  // segundos sin actividad para dormirse

Mood moodFrom(const PetInput& in) {
  if (!in.connected) return Mood::Sleeping;
  const float load = max(in.cpu, in.gpu);
  if (load >= 85) return Mood::Stressed;
  // Trabaja cuando tú trabajas. Sin ese dato (Linux), se guía por la carga del PC.
  const bool working = in.idle >= 0 ? in.idle < WORKING_IDLE_S : load >= 20;
  if (working) return load >= 55 ? Mood::Busy : Mood::Working;
  if (in.idle >= SLEEP_IDLE_S) return Mood::Sleeping;
  return Mood::Relaxed;
}

// Los humores, contados a su manera: siempre conectada a algo.
const char* moodName(Mood m) {
  switch (m) {
    case Mood::Sleeping: return "desconectada";
    case Mood::Relaxed: return "despierta";
    case Mood::Working: return "conectada";
    case Mood::Busy: return "a tope";
    case Mood::Stressed: return "saturada";
  }
  return "";
}
