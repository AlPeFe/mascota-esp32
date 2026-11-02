#pragma once
#include "effects.h"
#include "pet.h"
#include "polymodel.h"

// Mascota low poly sentada a su escritorio, mirando al frente, con el teclado
// delante. Todas comparten cuerpo y animaciones; cambian la cabeza, la cola y
// los colores. Teclea cuando tú trabajas.
class Critter : public Pet {
 public:
  // Qué se construye sobre el cuerpo: un bicho (gato, conejo) o una chica.
  enum Kind : uint8_t { Beast, Human };

  struct Species {
    Kind kind;
    bool longEars;  // orejas largas (conejo) o de punta (gato)
    bool longTail;  // cola larga o pompón
    float scale;    // para que quepan las orejas largas
    // Pelo, interior de las orejas, camiseta, raya, pantalón y zapatos.
    uint8_t fur[3], inner[3], shirt[3], stripe[3], shorts[3], shoe[3];
    uint8_t hair[3], accent[3];  // melena y cinta (solo para Human)
  };

  explicit Critter(const Species& species);

  void update(const PetInput& in, float dt) override;
  void draw(Gfx& g) override;
  void onTap(int x, int y) override;
  Mood mood() const override { return mood_; }
  void modelStats(int& verts, int& tris, int& parts, int& maxV, int& maxT, int& maxP) const override {
    verts = model_.vertCount();
    tris = model_.triCount();
    parts = model_.partCount();
    maxV = PolyModel::maxVerts();
    maxT = PolyModel::maxTris();
    maxP = PolyModel::maxParts();
  }

 private:
  static const int FLASH_COUNT = 2;

  void buildDesk(int parent);
  void buildBody(int parent);
  void buildHair();
  void buildFace();
  void showFace();
  void updateKeys(float dt);
  bool human() const { return species_.kind == Human; }

  const Species species_;
  PolyModel model_;
  Effects fx_;
  Mood mood_ = Mood::Sleeping;
  float t_ = 0;
  bool dirty_ = true;

  // Piezas del modelo
  int body_, head_, armL_, armR_, shinL_, shinR_, tail_;
  int hair_, bangs_, strand_, ribbon_;
  int deskMug_, handMug_, handProp_;
  int keyFlash_[FLASH_COUNT];
  int eyesOpen_, eyesBlink_, eyesClosed_, eyesHappy_, eyesFocus_, eyesStress_;
  int mouthSmall_, mouthOpen_, mouthRound_, mouthFlat_, mouthWavy_;
  int blush_;

  // Salto en la silla al acariciarla
  float air_ = 0, airVel_ = 0;

  // Postura actual (se acerca poco a poco a la del humor)
  float headPitch_ = 0, headRoll_ = 0, headYaw_ = 0;
  float armLZ_ = 0.15f, armRZ_ = -0.15f, armLX_ = -1.47f, armRX_ = -1.47f;
  float feet_ = 0;

  // Tecleo
  float typing_ = 0, typePhase_ = 0;
  float pauseTimer_ = 0, nextPause_ = 4;  // se para a pensar de vez en cuando
  float flashTimer_ = 0;

  // Café y cigarro, por turnos y al azar
  float sipTimer_ = 0, smokeTimer_ = 0, nextBreak_ = 3;
  bool nextIsSmoke_ = false;
  bool dragging_ = false, exhaling_ = false;  // calada y soltar el humo
  float tipSmokeTimer_ = 0, exhaleTimer_ = 0;

  float heat_ = 0;

  // Cara
  float blink_ = 0, nextBlink_ = 3;
  float lookYaw_ = 0, lookPitch_ = 0, lookTimer_ = 2;
  float petTimer_ = 0;

  // Temporizadores de partículas
  float zzzTimer_ = 0, sweatTimer_ = 0, steamTimer_ = 0;
};
