// Mascota de escritorio + monitor de recursos del PC
// Placa: ESP32-2432S028 ("Cheap Yellow Display"), 2.8" 320x240 táctil.
// El PC envía los datos por USB con mascota-monitor (carpeta monitor/).
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "app.h"
#include "config.h"
#include "gfx.h"
#include "pets/pet.h"
#include "screens/screen.h"
#include "settings.h"
#include "stats.h"
#include "ui.h"

static TFT_eSPI tft;
static TFT_eSprite strip(&tft);
static Gfx gfx(strip);
static int stripH = STRIP_H;
static bool stripReady = false;
static uint32_t renderMs = 0;

Layout layout;

static SPIClass touchSpi(VSPI);
static XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

static const int BL_CHANNEL = 0;

static Pet* pet = nullptr;
static Screen* screens[PAGE_COUNT];
static int page = 0;
static bool dirty = true;
static uint32_t lastFrame = 0, lastUpdate = 0, lastTouchMs = 0;
static int lastTapX = -1, lastTapY = -1;
static uint8_t rotation = 0;

// --- Funciones para las pantallas (app.h) ---

Pet* appPet() { return pet; }
int appPage() { return page; }

void appSetPet(int index) {
  delete pet;
  pet = PETS[constrain(index, 0, PET_COUNT - 1)].create();
  dirty = true;
}

// La franja tiene el ancho de la pantalla, así que se rehace al girar.
// Si no hay memoria para 80 filas, se usan franjas de 40.
static void createStrip() {
  strip.deleteSprite();
  strip.setColorDepth(16);
  stripH = STRIP_H;
  if (!strip.createSprite(SCREEN_W, stripH)) {
    stripH = STRIP_H / 2;
    strip.createSprite(SCREEN_W, stripH);
  }
}

void appApplyDisplay() {
  // 0 y 2 son vertical, 1 y 3 horizontal; el giro suma media vuelta
  rotation = (settings.vertical ? 0 : 1) + (settings.flip ? 2 : 0);
  tft.setRotation(rotation);
  tft.invertDisplay(settings.invert);

  layout.w = tft.width();
  layout.h = tft.height();
  layout.portrait = settings.vertical;
  layout.floorY = layout.portrait ? 229 : 232;
  layout.petX = layout.portrait ? 120 : 228;
  layout.petScale = layout.portrait ? 1.0f : 0.88f;

  if (stripReady) createStrip();
  dirty = true;
}

// --- Brillo: baja solo cuando no hay PC y nadie toca la pantalla ---

static void updateBacklight() {
  static int current = -1;
  uint32_t now = millis();
  bool idle = now - stats.lastMs > DIM_AFTER_MS && now - lastTouchMs > 15000;
  int target = idle ? max(10, settings.brightness / 5) : settings.brightness;
  if (target != current) {
    ledcWrite(BL_CHANNEL, target);
    current = target;
  }
}

// --- Táctil ---

static bool readTouch(int& x, int& y) {
  if (!touch.touched()) return false;
  // El táctil se lee siempre en la misma posición (rotación 1) y aquí se
  // pasa a la orientación actual, para que la calibración sirva en todas.
  TS_Point p = touch.getPoint();
  float u = constrain((p.x - TOUCH_X_MIN) / (float)(TOUCH_X_MAX - TOUCH_X_MIN), 0.0f, 1.0f);
  float v = constrain((p.y - TOUCH_Y_MIN) / (float)(TOUCH_Y_MAX - TOUCH_Y_MIN), 0.0f, 1.0f);
  float nx, ny;
  switch (rotation) {
    case 0: nx = 1 - v; ny = u; break;
    case 1: nx = u; ny = v; break;
    case 2: nx = v; ny = 1 - u; break;
    default: nx = 1 - u; ny = 1 - v; break;
  }
  x = (int)(nx * (SCREEN_W - 1));
  y = (int)(ny * (SCREEN_H - 1));
  return true;
}

static void goPage(int p) {
  page = (p + PAGE_COUNT) % PAGE_COUNT;
  dirty = true;
}

static void handleTap(int x, int y) {
  lastTapX = x;
  lastTapY = y;
  if (y < TOPBAR_H + 4) {
    if (x < SCREEN_W / 3) goPage(page - 1);
    else if (x > SCREEN_W * 2 / 3) goPage(page + 1);
  } else {
    screens[page]->onTap(x, y);
    dirty = true;
  }
}

// Distingue toque y deslizamiento al levantar el dedo.
static void handleTouch() {
  static bool down = false;
  static int x0, y0, x1, y1;
  static uint32_t lastSeen;

  int x, y;
  uint32_t now = millis();
  if (readTouch(x, y)) {
    if (!down) {
      down = true;
      x0 = x;
      y0 = y;
    }
    x1 = x;
    y1 = y;
    lastSeen = lastTouchMs = now;
    return;
  }
  if (!down || now - lastSeen < 80) return;  // pequeño margen contra rebotes

  down = false;
  int dx = x1 - x0, dy = y1 - y0;
  if (abs(dx) > 70 && abs(dx) > 2 * abs(dy)) {
    goPage(page + (dx < 0 ? 1 : -1));
  } else if (abs(dx) < 25 && abs(dy) < 25) {
    handleTap(x0, y0);
  }
}

// --- Dibujo por franjas ---

static void drawStrip(int top) {
  gfx.top = top;
  gfx.height = min(stripH, SCREEN_H - top);
  strip.fillSprite(col::bg);
  screens[page]->draw(gfx);
}

static void render() {
  uint32_t start = millis();
  for (int top = 0; top < SCREEN_H; top += stripH) {
    drawStrip(top);
    strip.pushSprite(0, top);
  }
  renderMs = millis() - start;
}

// --- Órdenes de depuración por USB (las usa pc/captura.py) ---

// Envía la imagen por el puerto serie: "SHOT ancho alto" y los píxeles en
// RGB565. Tarda unos 13 s, así que mantiene viva la conexión mientras tanto.
static void sendScreenshot() {
  bool connected = statsConnected();
  Serial.printf("SHOT %d %d\n", SCREEN_W, SCREEN_H);
  for (int top = 0; top < SCREEN_H; top += stripH) {
    if (connected) stats.lastMs = millis();
    drawStrip(top);
    Serial.write((const uint8_t*)strip.getPointer(), SCREEN_W * gfx.height * 2);
  }
  Serial.flush();
  if (connected) stats.lastMs = millis();
}

static void handleCommand(const char* cmd) {
  int a, b;
  if (strcmp(cmd, "!shot") == 0) {
    sendScreenshot();
  } else if (strcmp(cmd, "!info") == 0) {
    Serial.printf("INFO w=%d h=%d rot=%d page=%d stripH=%d renderMs=%u heap=%u maxBlock=%u tap=%d,%d dataAgeMs=%u cpu=%.1f ram=%.1f gpu=%s idle=%d\n",
                  SCREEN_W, SCREEN_H, rotation, page, stripH, renderMs, ESP.getFreeHeap(),
                  ESP.getMaxAllocHeap(), lastTapX, lastTapY, millis() - stats.lastMs, stats.cpu, stats.ram, stats.gpuName, stats.idle);
  } else if (sscanf(cmd, "!page %d", &a) == 1) {
    goPage(a);
  } else if (sscanf(cmd, "!tap %d %d", &a, &b) == 2) {
    handleTap(a, b);
  }
}

// Pantalla de inicio con prueba de colores. Tocar invierte los colores
// (algunas placas traen la pantalla con los colores invertidos).
static void splash() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(col::text);
  tft.setTextSize(3);
  tft.drawString("Mascota", SCREEN_W / 2, 40, 1);
  tft.setTextSize(1);

  const char* names[3] = {"ROJO", "VERDE", "AZUL"};
  const uint16_t colors[3] = {TFT_RED, TFT_GREEN, TFT_BLUE};
  for (int i = 0; i < 3; i++) {
    int x = SCREEN_W / 2 - 30 + (i - 1) * SCREEN_W / 3;
    tft.fillRect(x, 75, 60, 50, colors[i]);
    tft.drawString(names[i], x + 30, 138, 1);
  }
  tft.setTextColor(col::muted);
  tft.drawString("Fondo negro o colores raros?", SCREEN_W / 2, 175, 1);
  tft.drawString("Toca para invertirlos", SCREEN_W / 2, 190, 1);

  uint32_t until = millis() + 1500;
  bool wasDown = false;
  while (millis() < until) {
    statsPoll();  // responde al saludo del PC mientras tanto
    int x, y;
    bool isDown = readTouch(x, y);
    if (isDown && !wasDown) {
      settings.invert = !settings.invert;
      tft.invertDisplay(settings.invert);
      settingsSave();
      until = millis() + 3000;
    }
    wasDown = isDown;
    delay(20);
  }
}

void setup() {
  // LED RGB trasero apagado (es activo a nivel bajo)
  for (int pin : {PIN_LED_R, PIN_LED_G, PIN_LED_B}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }

  statsBegin();
  settingsLoad();

  tft.init();
  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);
  appApplyDisplay();

  ledcSetup(BL_CHANNEL, 5000, 8);
  ledcAttachPin(PIN_BACKLIGHT, BL_CHANNEL);
  ledcWrite(BL_CHANNEL, settings.brightness);

  splash();

  createStrip();
  stripReady = true;

  appSetPet(settings.pet);
  screens[0] = makePetScreen();
  screens[1] = makeDetailScreen();
  screens[2] = makeTokensScreen();
  screens[3] = makeSettingsScreen();
  statsSetCommandHandler(handleCommand);
  lastUpdate = millis();
}

void loop() {
  if (statsPoll() && page != 0) dirty = true;
  handleTouch();

  uint32_t now = millis();
  float dt = min((now - lastUpdate) / 1000.0f, 0.1f);
  lastUpdate = now;

  PetInput in = {statsConnected(), stats.cpuSmooth, stats.gpu, stats.idle};
  pet->update(in, dt);
  screens[page]->update(dt);
  updateBacklight();

  if (dirty || now - lastFrame >= screens[page]->frameMs()) {
    render();
    lastFrame = now;
    dirty = false;
  }
}
