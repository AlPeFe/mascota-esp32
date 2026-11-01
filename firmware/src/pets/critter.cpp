#include "critter.h"

#include <Arduino.h>

#include "../ui.h"

using Ring = PolyModel::Ring;
using Vec3 = PolyModel::Vec3;

namespace {

enum Color : uint8_t {
  C_FUR, C_INNER, C_SHIRT, C_STRIPE, C_SHORTS, C_SHOE,
  C_INK, C_WHITE, C_BLUSH, C_MOUTH, C_NOSE,
  C_DESK, C_DESK_LEG, C_CHAIR, C_KEYBOARD, C_KEY, C_CRT, C_CRT_DARK, C_MUG, C_FLASH,
};

// La cámara mira un poco desde arriba, para que se vea la mesa y el teclado
const float CAMERA_PITCH = 0.38f;
const float CAMERA_PIVOT_Y = 70;

// Cuerpo sentado
const float NECK_Y = 80, SHOULDER_Y = 72, HIP_Y = 38;
const float THIGH_ANGLE = 1.45f, THIGH_LENGTH = 20;

// Cabeza, respecto al cuello
const float HEAD_Y = 40, HEAD_RX = 52, HEAD_RY = 40, HEAD_RZ = 42;
const float FACE_SCALE = HEAD_RX / 66;  // la cara se diseñó para una cabeza de 66 de radio

// Escritorio y teclado
const float DESK_Y = 61;
const float KEYBOARD_Z0 = 26, KEYBOARD_Z1 = 50, KEYBOARD_HALF_W = 42, KEYBOARD_H = 3;
const int KEY_ROWS = 3, KEY_COLS = 9;

// Brazo apoyado en el teclado
const float REST_ARM_X = -1.47f, REST_ARM_Z = 0.15f;

const float GRAVITY = 700;

float rnd(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }

float approach(float v, float target, float rate, float dt) { return v + (target - v) * min(1.0f, rate * dt); }

// Punto sobre la cara: la Z sigue más o menos la curva de la cabeza.
Vec3 onFace(float x, float y) {
  x *= FACE_SCALE;
  y *= FACE_SCALE;
  float nx = x / HEAD_RX, ny = y / HEAD_RY;
  float k = sqrtf(max(0.15f, 1 - nx * nx - ny * ny * 0.5f));
  return {x, HEAD_Y + y, HEAD_RZ * k * 1.03f};
}

// Polígono ovalado pegado a la cara.
void faceOval(PolyModel& m, int part, float cx, float cy, float rx, float ry, int sides, uint8_t color) {
  Vec3 pts[12];
  for (int i = 0; i < sides; i++) {
    float a = (i + 0.5f) * TWO_PI / sides;
    pts[i] = onFace(cx + rx * cosf(a), cy + ry * sinf(a));
  }
  m.poly(part, pts, sides, color);
}

// Trazo grueso pegado a la cara, de un punto al siguiente.
void faceStroke(PolyModel& m, int part, const float* xy, int points, float width, uint8_t color) {
  for (int i = 0; i + 1 < points; i++) {
    float x0 = xy[i * 2], y0 = xy[i * 2 + 1], x1 = xy[i * 2 + 2], y1 = xy[i * 2 + 3];
    float dx = x1 - x0, dy = y1 - y0, len = max(sqrtf(dx * dx + dy * dy), 0.01f);
    float nx = -dy / len * width / 2, ny = dx / len * width / 2;
    Vec3 quad[4] = {onFace(x0 - nx, y0 - ny), onFace(x1 - nx, y1 - ny), onFace(x1 + nx, y1 + ny),
                    onFace(x0 + nx, y0 + ny)};
    m.poly(part, quad, 4, color);
  }
}

// Tecla: un cuadrado horizontal.
void flatQuad(PolyModel& m, int part, float x, float y, float z, float halfW, float halfD, uint8_t color) {
  Vec3 q[4] = {{x - halfW, y, z + halfD}, {x + halfW, y, z + halfD}, {x + halfW, y, z - halfD}, {x - halfW, y, z - halfD}};
  m.poly(part, q, 4, color, {0, 1, 0});
}

float keyX(int col) { return -KEYBOARD_HALF_W + 5 + col * (2 * KEYBOARD_HALF_W - 10) / (KEY_COLS - 1); }
float keyZ(int row) { return KEYBOARD_Z0 + 5 + row * (KEYBOARD_Z1 - KEYBOARD_Z0 - 10) / (KEY_ROWS - 1); }

void addMug(PolyModel& m, int part) {
  const Ring r[] = {{0, 8, 8, C_MUG}, {-14, 7, 7, C_MUG}};
  m.lathe(part, r, 2, 6);
}

}  // namespace

Critter::Critter(const Species& species) : species_(species) {
  auto setRgb = [&](uint8_t c, const uint8_t* v) { model_.setColor(c, v[0], v[1], v[2]); };
  setRgb(C_FUR, species_.fur);
  setRgb(C_INNER, species_.inner);
  setRgb(C_SHIRT, species_.shirt);
  setRgb(C_STRIPE, species_.stripe);
  setRgb(C_SHORTS, species_.shorts);
  setRgb(C_SHOE, species_.shoe);
  model_.setColor(C_INK, 34, 30, 46);
  model_.setColor(C_WHITE, 255, 255, 255);
  model_.setColor(C_BLUSH, 244, 150, 150);
  model_.setColor(C_MOUTH, 110, 40, 60);
  model_.setColor(C_NOSE, 225, 110, 120);
  model_.setColor(C_DESK, 216, 220, 230);
  model_.setColor(C_DESK_LEG, 150, 156, 172);
  model_.setColor(C_CHAIR, 92, 98, 120);
  model_.setColor(C_KEYBOARD, 120, 126, 146);
  model_.setColor(C_KEY, 240, 240, 245);
  model_.setColor(C_CRT, 232, 226, 208);
  model_.setColor(C_CRT_DARK, 196, 190, 172);
  model_.setColor(C_MUG, 235, 90, 90);
  model_.setColor(C_FLASH, 0, 170, 150);

  // La cámara es la pieza raíz: al girarla se ve todo un poco desde arriba
  int camera = model_.addPart(-1, 0, CAMERA_PIVOT_Y, 0);
  model_.part(camera).rx = CAMERA_PITCH;
  int world = model_.addPart(camera, 0, -CAMERA_PIVOT_Y, 0);

  buildDesk(world);
  buildBody(world);
  buildFace();
}

void Critter::buildDesk(int parent) {
  // Silla, detrás de todo
  int chair = model_.addPart(parent, 0, 0, 0, -40);
  model_.box(chair, -18, 18, 0, 6, -14, 12, C_CHAIR);
  model_.box(chair, -4, 4, 6, 28, -5, 3, C_DESK_LEG);
  model_.box(chair, -24, 24, 28, 35, -22, 18, C_CHAIR);
  model_.box(chair, -22, 22, 42, 92, -30, -25, C_CHAIR);

  // Mesa
  int desk = model_.addPart(parent, 0, 0, 0);
  model_.box(desk, -92, 92, DESK_Y - 5, DESK_Y, 14, 66, C_DESK);
  model_.box(desk, -86, -78, 0, DESK_Y - 5, 22, 58, C_DESK_LEG);
  model_.box(desk, 78, 86, 0, DESK_Y - 5, 22, 58, C_DESK_LEG);

  // Teclado con sus teclas
  int keyboard = model_.addPart(parent, 0, 0, 0, 20);
  model_.box(keyboard, -KEYBOARD_HALF_W, KEYBOARD_HALF_W, DESK_Y, DESK_Y + KEYBOARD_H, KEYBOARD_Z0, KEYBOARD_Z1, C_KEYBOARD);
  int keys = model_.addPart(parent, 0, 0, 0, 30, true);
  for (int row = 0; row < KEY_ROWS; row++)
    for (int col = 0; col < KEY_COLS; col++)
      flatQuad(model_, keys, keyX(col), DESK_Y + KEYBOARD_H + 0.3f, keyZ(row), 3.4f, 2.6f, C_KEY);
  for (int i = 0; i < FLASH_COUNT; i++) {
    keyFlash_[i] = model_.addPart(parent, 0, 0, 0, 34, true);
    flatQuad(model_, keyFlash_[i], 0, 0, 0, 3.4f, 2.6f, C_FLASH);
  }

  // Ratón
  int mouse = model_.addPart(parent, 0, 0, 0, 20);
  model_.box(mouse, 52, 60, DESK_Y, DESK_Y + 3, 34, 46, C_KEYBOARD);

  // Monitor CRT, girado hacia la mascota: desde aquí se ve por detrás
  int crt = model_.addPart(parent, -73, DESK_Y, 40, 16);
  model_.part(crt).ry = 2.5f;
  model_.box(crt, -9, 9, 0, 5, -8, 8, C_CRT_DARK);
  model_.box(crt, -22, 22, 5, 43, 0, 10, C_CRT);
  model_.box(crt, -15, 15, 9, 39, -20, 0, C_CRT_DARK);

  // Taza
  deskMug_ = model_.addPart(parent, 72, DESK_Y + 14, 28, 18);
  addMug(model_, deskMug_);
}

void Critter::buildBody(int parent) {
  body_ = model_.addPart(parent, 0, 0, 0);

  // Tronco: camiseta con una raya y pantalón corto
  const Ring torso[] = {{NECK_Y, 13, 11, C_SHIRT}, {70, 20, 15, C_STRIPE}, {61, 23, 17, C_SHIRT},
                        {46, 24, 18, C_SHORTS},    {34, 22, 17, C_SHORTS}};
  model_.lathe(body_, torso, 5, 6);

  // Cola, asomando por un lado de la silla
  tail_ = model_.addPart(body_, 16, 42, -14, -10);
  if (species_.longTail) {
    const Ring t[] = {{42, 2.5f, 2.5f, C_FUR, 32, -4}, {30, 4, 4, C_FUR, 26, -4}, {14, 5, 5, C_FUR, 12, -2}, {0, 5, 5, C_FUR, 0, 0}};
    model_.lathe(tail_, t, 4, 4);
  } else {
    const Ring t[] = {{9, 5, 5, C_FUR, 8, -8}, {4, 10, 10, C_FUR, 8, -8}, {-4, 10, 10, C_FUR, 8, -8}, {-9, 5, 5, C_FUR, 8, -8}};
    model_.lathe(tail_, t, 4, 6);
  }

  // Piernas sentadas: muslo hacia delante y espinilla colgando con su zapato
  for (int side = -1; side <= 1; side += 2) {
    int thigh = model_.addPart(body_, side * 12.0f, HIP_Y, 6);
    model_.part(thigh).rx = -THIGH_ANGLE;
    const Ring upper[] = {{0, 10, 10, C_SHORTS}, {-THIGH_LENGTH, 8.5f, 8.5f, C_FUR}};
    model_.lathe(thigh, upper, 2, 5);

    int shin = model_.addPart(thigh, 0, -THIGH_LENGTH, 0);
    const Ring lower[] = {{0, 7.5f, 7.5f, C_FUR},      {-22, 7, 7, C_SHOE},        {-24, 10, 11, C_SHOE, 0, 3},
                          {-31, 11, 14, C_SHOE, 0, 4}, {-34, 10, 13, C_SHOE, 0, 4}};
    model_.lathe(shin, lower, 5, 5);
    (side < 0 ? shinL_ : shinR_) = shin;
  }

  // Brazos con mano redonda. El pivote está en el hombro.
  for (int side = -1; side <= 1; side += 2) {
    int arm = model_.addPart(body_, side * 21.0f, SHOULDER_Y, 0, 60);
    const Ring r[] = {{3, 6, 6, C_SHIRT}, {-12, 7, 7, C_FUR}, {-22, 5.5f, 5.5f, C_FUR},
                      {-25, 8.5f, 8.5f, C_FUR}, {-36, 7.5f, 7.5f, C_FUR}, {-41, 4, 4, C_FUR}};
    model_.lathe(arm, r, 6, 5);
    (side < 0 ? armL_ : armR_) = arm;
  }

  // Taza en la mano: solo se ve mientras bebe
  handMug_ = model_.addPart(armR_, 0, -34, 8, 66);
  model_.part(handMug_).rx = 2.35f;
  addMug(model_, handMug_);

  // Cabeza octogonal, con el pivote en el cuello
  head_ = model_.addPart(body_, 0, NECK_Y, 0, 22);
  const Ring h[] = {
      {HEAD_Y + HEAD_RY, HEAD_RX * 0.52f, HEAD_RZ * 0.52f, C_FUR}, {HEAD_Y + HEAD_RY * 0.62f, HEAD_RX * 0.90f, HEAD_RZ * 0.90f, C_FUR},
      {HEAD_Y + HEAD_RY * 0.10f, HEAD_RX, HEAD_RZ, C_FUR},         {HEAD_Y - HEAD_RY * 0.45f, HEAD_RX * 0.95f, HEAD_RZ * 0.95f, C_FUR},
      {HEAD_Y - HEAD_RY * 0.85f, HEAD_RX * 0.65f, HEAD_RZ * 0.65f, C_FUR}, {HEAD_Y - HEAD_RY, HEAD_RX * 0.40f, HEAD_RZ * 0.40f, C_FUR}};
  model_.lathe(head_, h, 6, 8);

  // Orejas: salen de la parte alta de la cabeza
  for (int side = -1; side <= 1; side += 2) {
    int ear = model_.addPart(head_, side * 27.0f, HEAD_Y + HEAD_RY * 0.70f, 0, -6);
    model_.part(ear).rz = -side * (species_.longEars ? 0.12f : 0.32f);
    int inner = model_.addPart(ear, 0, 0, 0, 10, true);
    if (species_.longEars) {
      const Ring r[] = {{62, 3, 3, C_FUR}, {48, 8, 6, C_FUR}, {22, 12, 8, C_FUR}, {0, 9, 7, C_FUR}};
      model_.lathe(ear, r, 4, 6);
      Vec3 p[] = {{-5, 5, 8}, {5, 5, 8}, {7, 22, 9}, {4, 46, 7}, {-4, 46, 7}, {-7, 22, 9}};
      model_.poly(inner, p, 6, C_INNER);
    } else {
      const Ring r[] = {{36, 1, 1, C_FUR}, {0, 17, 11, C_FUR}};
      model_.lathe(ear, r, 2, 5);
      Vec3 p[] = {{-9, 4, 9}, {9, 4, 9}, {0, 27, 3}};
      model_.poly(inner, p, 3, C_INNER);
    }
  }
}

void Critter::buildFace() {
  // Las "pegatinas" se ordenan siempre por delante de la cabeza
  auto decal = [&]() { return model_.addPart(head_, 0, 0, 0, 80, true); };

  int fixed = decal();
  const float noseY = -9;
  Vec3 nose[] = {onFace(-4, noseY + 3), onFace(0, noseY - 3), onFace(4, noseY + 3)};
  // Sentido antihorario: izquierda, abajo, derecha
  model_.poly(fixed, nose, 3, C_NOSE);

  blush_ = decal();
  faceOval(model_, blush_, -42, -14, 10, 6, 6, C_BLUSH);
  faceOval(model_, blush_, 42, -14, 10, 6, 6, C_BLUSH);

  eyesOpen_ = decal();
  eyesBlink_ = decal();
  eyesClosed_ = decal();
  eyesHappy_ = decal();
  eyesFocus_ = decal();
  eyesStress_ = decal();

  for (int side = -1; side <= 1; side += 2) {
    const float ex = side * 27.0f, ey = 6;

    faceOval(model_, eyesOpen_, ex, ey, 12, 15, 8, C_INK);
    // El brillo es hijo de los ojos abiertos: se ve solo cuando ellos se ven
    int shine = model_.addPart(eyesOpen_, 0, 0, 0, 90, true);
    faceOval(model_, shine, ex - 3, ey + 6, 4, 5, 5, C_WHITE);

    const float blink[] = {ex - 12, ey - 2, ex + 12, ey - 2};
    faceStroke(model_, eyesBlink_, blink, 2, 4, C_INK);

    const float closed[] = {ex - 12, ey + 2, ex - 5, ey - 4, ex + 5, ey - 4, ex + 12, ey + 2};
    faceStroke(model_, eyesClosed_, closed, 4, 3.5f, C_INK);

    const float happy[] = {ex - 12, ey - 4, ex, ey + 8, ex + 12, ey - 4};
    faceStroke(model_, eyesHappy_, happy, 3, 4.5f, C_INK);

    faceOval(model_, eyesFocus_, ex, ey - 4, 11, 9, 8, C_INK);
    const float brow[] = {ex + side * 13, ey + 15, ex - side * 11, ey + 7};
    faceStroke(model_, eyesFocus_, brow, 2, 4, C_INK);

    const float stress[] = {ex + side * 11, ey + 9, ex - side * 9, ey, ex + side * 11, ey - 9};
    faceStroke(model_, eyesStress_, stress, 3, 4.5f, C_INK);
  }

  mouthSmall_ = decal();
  mouthOpen_ = decal();
  mouthRound_ = decal();
  mouthFlat_ = decal();
  mouthWavy_ = decal();

  const float my = -20;
  const float small[] = {-11, my + 3, -5.5f, my - 2, 0, my + 2, 5.5f, my - 2, 11, my + 3};
  faceStroke(model_, mouthSmall_, small, 5, 3, C_INK);

  // Boca abierta: media luna hacia abajo
  Vec3 open[7];
  for (int i = 0; i < 7; i++) {
    float a = PI + PI * i / 6;
    open[i] = onFace(11 * cosf(a), my + 3 + 11 * sinf(a));
  }
  model_.poly(mouthOpen_, open, 7, C_MOUTH);

  faceOval(model_, mouthRound_, 0, my - 2, 7, 7, 7, C_MOUTH);

  const float flat[] = {-9, my, 9, my};
  faceStroke(model_, mouthFlat_, flat, 2, 3, C_INK);

  const float wavy[] = {-14, my - 2, -7, my + 3, 0, my - 2, 7, my + 3, 14, my - 2};
  faceStroke(model_, mouthWavy_, wavy, 5, 3, C_INK);
}

void Critter::showFace() {
  enum class Eyes { Open, Closed, Happy, Focus, Stress } eyes;
  enum class Mouth { Small, Open, Round, Flat, Wavy } mouth;

  if (petTimer_ > 0) {
    eyes = Eyes::Happy;
    mouth = Mouth::Open;
  } else {
    switch (mood_) {
      case Mood::Sleeping: eyes = Eyes::Closed; mouth = Mouth::Round; break;
      case Mood::Stressed: eyes = Eyes::Stress; mouth = Mouth::Wavy; break;
      case Mood::Busy: eyes = Eyes::Focus; mouth = Mouth::Flat; break;
      case Mood::Relaxed: eyes = Eyes::Open; mouth = sipTimer_ > 0 ? Mouth::Round : Mouth::Open; break;
      default: eyes = Eyes::Open; mouth = Mouth::Small; break;
    }
  }

  const bool blinking = blink_ > 0;
  model_.part(eyesOpen_).visible = eyes == Eyes::Open && !blinking;
  model_.part(eyesBlink_).visible = (eyes == Eyes::Open || eyes == Eyes::Focus) && blinking;
  model_.part(eyesClosed_).visible = eyes == Eyes::Closed;
  model_.part(eyesHappy_).visible = eyes == Eyes::Happy;
  model_.part(eyesFocus_).visible = eyes == Eyes::Focus && !blinking;
  model_.part(eyesStress_).visible = eyes == Eyes::Stress;

  model_.part(mouthSmall_).visible = mouth == Mouth::Small;
  model_.part(mouthOpen_).visible = mouth == Mouth::Open;
  model_.part(mouthRound_).visible = mouth == Mouth::Round;
  model_.part(mouthFlat_).visible = mouth == Mouth::Flat;
  model_.part(mouthWavy_).visible = mouth == Mouth::Wavy;

  model_.part(blush_).visible = petTimer_ > 0 || mood_ == Mood::Relaxed || mood_ == Mood::Stressed;
}

// Las teclas que se van iluminando mientras teclea.
void Critter::updateKeys(float dt) {
  const bool pressing = typing_ > 0.5f && petTimer_ <= 0;
  flashTimer_ -= dt;
  if (pressing && flashTimer_ <= 0) {
    flashTimer_ = mood_ == Mood::Working ? 0.11f : 0.06f;
    for (int i = 0; i < FLASH_COUNT; i++) {
      model_.part(keyFlash_[i]).pivot = {keyX(random(KEY_COLS)), DESK_Y + KEYBOARD_H + 0.5f, keyZ(random(KEY_ROWS))};
    }
  }
  for (int i = 0; i < FLASH_COUNT; i++) model_.part(keyFlash_[i]).visible = pressing;
}

void Critter::update(const PetInput& in, float dt) {
  t_ += dt;
  dirty_ = true;
  mood_ = moodFrom(in);
  const bool sleeping = mood_ == Mood::Sleeping;
  const bool relaxed = mood_ == Mood::Relaxed;
  const float load = in.connected ? max(in.cpu, in.gpu) : 0;  // sin PC los datos ya no valen

  heat_ = approach(heat_, constrain((load - 70) / 25, 0.0f, 1.0f), 1.0f, dt);

  // --- Salto en la silla ---
  if (air_ > 0 || airVel_ > 0) {
    airVel_ -= GRAVITY * dt;
    air_ += airVel_ * dt;
    if (air_ <= 0) {
      air_ = 0;
      airVel_ = 0;
    }
  }

  // --- Mirar: casi siempre al frente; más distraída cuando descansa ---
  lookTimer_ -= dt;
  if (lookTimer_ <= 0) {
    const float range = relaxed ? 0.5f : 0.1f;
    lookYaw_ = rnd(-range, range);
    lookPitch_ = rnd(-0.06f, 0.08f);
    lookTimer_ = rnd(1.5f, 4.5f);
  }
  nextBlink_ -= dt;
  if (nextBlink_ <= 0) {
    blink_ = 0.14f;
    nextBlink_ = rnd(2.5f, 6.0f);
  }
  blink_ = max(0.0f, blink_ - dt);
  petTimer_ = max(0.0f, petTimer_ - dt);

  // --- Teclear: a ráfagas, parándose a pensar de vez en cuando ---
  const bool wantsToType = mood_ == Mood::Working || mood_ == Mood::Busy || mood_ == Mood::Stressed;
  if (mood_ == Mood::Working) {
    nextPause_ -= dt;
    if (nextPause_ <= 0) {
      pauseTimer_ = rnd(0.5f, 1.4f);
      nextPause_ = rnd(3.0f, 7.0f);
    }
  } else {
    pauseTimer_ = 0;
  }
  pauseTimer_ = max(0.0f, pauseTimer_ - dt);
  typing_ = approach(typing_, wantsToType && pauseTimer_ <= 0 ? 1.0f : 0.0f, 10, dt);
  typePhase_ += dt * (mood_ == Mood::Working ? 16 : mood_ == Mood::Busy ? 25 : 34);
  updateKeys(dt);

  // --- Café, cuando descansa ---
  sipTimer_ = max(0.0f, sipTimer_ - dt);
  if (relaxed && petTimer_ <= 0) {
    nextSip_ -= dt;
    if (nextSip_ <= 0) {
      sipTimer_ = 2.6f;
      nextSip_ = rnd(7, 14);
    }
  } else {
    sipTimer_ = 0;
  }

  // --- Postura según el humor ---
  const float keystroke = 0.14f * typing_;
  float aLX = REST_ARM_X + keystroke * sinf(typePhase_);
  float aRX = REST_ARM_X - keystroke * sinf(typePhase_ * 1.13f);
  float aLZ = REST_ARM_Z + 0.04f * typing_ * sinf(typePhase_ * 0.37f);
  float aRZ = -REST_ARM_Z + 0.04f * typing_ * sinf(typePhase_ * 0.31f);
  float pitch = lookPitch_, roll = 0, turn = lookYaw_, feet = 0;
  float shake = 0;
  switch (mood_) {
    case Mood::Sleeping:
      pitch = 0.42f;
      roll = 0.12f + 0.03f * sinf(t_ * 0.9f);
      turn = 0;
      break;
    case Mood::Relaxed:
      feet = 0.3f * sinf(t_ * 2.4f);  // balancea los pies
      if (sipTimer_ > 0) {
        aRX = -2.35f;
        aRZ = -0.3f;
        pitch = -0.08f;
        turn = 0;
      }
      break;
    case Mood::Working: break;
    case Mood::Busy:
      pitch = 0.05f;
      turn = 0;
      break;
    case Mood::Stressed:
      roll = 0.05f * sinf(t_ * 18.0f);
      turn = 0;
      shake = 1.3f * sinf(t_ * 37.0f);
      break;
  }
  if (petTimer_ > 0) {
    aLX = aRX = 0;
    aLZ = -1.8f + 0.35f * sinf(t_ * 12.0f);
    aRZ = 1.8f - 0.35f * sinf(t_ * 12.0f);
    pitch = -0.04f;
    roll = 0.08f * sinf(t_ * 8.0f);
    turn = 0;
    feet = 0.4f * sinf(t_ * 10.0f);
  }

  const float rate = 12;
  armLZ_ = approach(armLZ_, aLZ, rate, dt);
  armRZ_ = approach(armRZ_, aRZ, rate, dt);
  armLX_ = approach(armLX_, aLX, rate, dt);
  armRX_ = approach(armRX_, aRX, rate, dt);
  feet_ = approach(feet_, feet, 10, dt);
  headPitch_ = approach(headPitch_, pitch, 6, dt);
  headRoll_ = approach(headRoll_, roll, 6, dt);
  headYaw_ = approach(headYaw_, turn, 5, dt);

  // Respira con la cabeza; el cuerpo tiembla y salta, la mesa no
  const float breathSpeed = sleeping ? 1.8f : 2.5f;
  model_.part(body_).pivot = {shake, air_, 0};
  model_.part(head_).pivot = {0, NECK_Y + 1.2f * sinf(t_ * breathSpeed), 0};
  // La cabeza compensa la inclinación de la cámara para mirar de frente
  model_.part(head_).rx = headPitch_ - CAMERA_PITCH;
  model_.part(head_).ry = headYaw_;
  model_.part(head_).rz = headRoll_;
  model_.part(armL_).rz = armLZ_;
  model_.part(armL_).rx = armLX_;
  model_.part(armR_).rz = armRZ_;
  model_.part(armR_).rx = armRX_;
  model_.part(shinL_).rx = THIGH_ANGLE - feet_;
  model_.part(shinR_).rx = THIGH_ANGLE + feet_;
  model_.part(tail_).rz = 0.14f * sinf(t_ * (sleeping ? 0.8f : 2.6f));
  model_.part(deskMug_).visible = sipTimer_ <= 0;
  model_.part(handMug_).visible = sipTimer_ > 0;

  // Se pone roja con el calor
  uint8_t fr = species_.fur[0] + (uint8_t)((246 - species_.fur[0]) * heat_);
  uint8_t fg = species_.fur[1] - (uint8_t)((species_.fur[1] - 110) * heat_);
  uint8_t fb = species_.fur[2] - (uint8_t)((species_.fur[2] - 95) * heat_);
  model_.setColor(C_FUR, fr, fg, fb);

  showFace();

  // --- Partículas ---
  const float scale = layout.petScale * species_.scale;
  const float x = layout.petX;
  const float headY = FLOOR_Y - (CAMERA_PIVOT_Y + (NECK_Y + HEAD_Y - CAMERA_PIVOT_Y) * cosf(CAMERA_PITCH)) * scale;
  const float headW = HEAD_RX * scale;
  const float topY = headY - HEAD_RY * scale;
  if (sleeping && (zzzTimer_ -= dt) <= 0) {
    zzzTimer_ = 1.6f;
    fx_.spawn(FxType::Zzz, x + headW * 0.9f, headY - 22, 0, -14, 2.4f, rnd(4, 9));
  }
  if (mood_ >= Mood::Busy && (sweatTimer_ -= dt) <= 0) {
    sweatTimer_ = mood_ == Mood::Stressed ? 0.6f : 2.0f;
    float side = rnd(0, 1) < 0.5f ? -1.0f : 1.0f;
    fx_.spawn(FxType::Sweat, x + side * headW * 0.85f, headY - 8, side * 30, -40, 0.9f, 3);
  }
  if (heat_ > 0.6f && (steamTimer_ -= dt) <= 0) {
    steamTimer_ = 0.32f;
    fx_.spawn(FxType::Steam, x + rnd(-24, 24), topY, rnd(-6, 6), -28, 1.1f, 2.5f);
  }
  fx_.update(dt);
}

void Critter::onTap(int x, int y) {
  const bool hit = x >= model_.minX() - 4 && x <= model_.maxX() + 4 && y >= model_.minY() - 4 && y <= model_.maxY() + 4;
  if (hit) {
    petTimer_ = 2.5f;
    airVel_ = 130;
    const float scale = layout.petScale * species_.scale;
    for (int i = 0; i < 3; i++)
      fx_.spawn(FxType::Heart, layout.petX + rnd(-45, 45) * scale, model_.minY() + rnd(10, 40), rnd(-15, 15), rnd(-45, -30), 1.6f, rnd(4, 7));
  } else if (mood_ != Mood::Sleeping) {
    lookYaw_ = x > layout.petX ? 0.45f : -0.45f;
    lookTimer_ = 2.5f;
  }
}

void Critter::draw(Gfx& g) {
  const float scale = layout.petScale * species_.scale;
  if (dirty_) {
    PolyModel::View view;
    view.x = layout.petX;
    view.floorY = FLOOR_Y;
    view.scale = scale;
    model_.prepare(view);
    dirty_ = false;
  }

  // Sombra: un polígono gris en el suelo
  const float srx = 104 * scale, sry = 7;
  const PsxVert center = {(int16_t)layout.petX, (int16_t)(FLOOR_Y - 2), 226, 228, 233};
  PsxVert prev = center;
  for (int i = 0; i <= 12; i++) {
    float a = i * TWO_PI / 12;
    PsxVert v = center;
    v.x = (int16_t)(layout.petX + srx * cosf(a));
    v.y = (int16_t)(FLOOR_Y - 2 + sry * sinf(a));
    if (i > 0) psxTriangle(g, center, prev, v, false);
    prev = v;
  }

  model_.draw(g);
  fx_.draw(g);
}
