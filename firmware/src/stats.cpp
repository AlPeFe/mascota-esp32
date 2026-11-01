#include "stats.h"

#include <ArduinoJson.h>

PcStats stats;

static char lineBuf[1536];
static size_t lineLen = 0;
static void (*commandHandler)(const char*) = nullptr;

void statsSetCommandHandler(void (*handler)(const char*)) { commandHandler = handler; }

void statsBegin() {
  Serial.setRxBufferSize(2048);
  Serial.begin(SERIAL_BAUD);
}

static bool parseResources(JsonDocument& doc) {
  stats.cpu = doc["cpu"] | 0.0f;
  stats.cpuSmooth = stats.valid ? stats.cpuSmooth * 0.6f + stats.cpu * 0.4f : stats.cpu;

  JsonArrayConst cores = doc["cores"].as<JsonArrayConst>();
  stats.coreCount = 0;
  for (JsonVariantConst v : cores) {
    if (stats.coreCount >= sizeof(stats.cores)) break;
    stats.cores[stats.coreCount++] = (uint8_t)constrain(v.as<int>(), 0, 100);
  }

  stats.ram = doc["ram"] | 0.0f;
  stats.ramUsed = doc["ramU"] | 0.0f;
  stats.ramTotal = doc["ramT"] | 0.0f;

  stats.gpu = doc["gpu"] | -1.0f;
  stats.vram = doc["vram"] | -1.0f;
  stats.vramUsed = doc["vramU"] | 0.0f;
  stats.vramTotal = doc["vramT"] | 0.0f;
  stats.gpuTemp = doc["gpuT"] | -1.0f;
  strlcpy(stats.gpuName, doc["gpuN"] | "GPU", sizeof(stats.gpuName));

  stats.disk = doc["disk"] | 0.0f;
  stats.diskRead = doc["dr"] | 0.0f;
  stats.diskWrite = doc["dw"] | 0.0f;
  stats.netDown = doc["dn"] | 0.0f;
  stats.netUp = doc["up"] | 0.0f;

  stats.hour = doc["hh"] | -1;
  stats.minute = doc["mm"] | -1;
  stats.idle = doc["idle"] | -1;

  stats.cpuHist[stats.histHead] = stats.cpu;
  stats.netHist[stats.histHead] = stats.netDown + stats.netUp;
  stats.histHead = (stats.histHead + 1) % HISTORY_LEN;

  stats.valid = true;
  stats.lastMs = millis();
  return true;
}

static bool handleLine(const char* line) {
  // Saludo: el monitor manda "?" para encontrar el puerto de la mascota.
  if (strcmp(line, "?") == 0) {
    Serial.println("MASCOTA_ESP32 " FIRMWARE_VERSION);
    return false;
  }
  if (line[0] == '!') {
    if (commandHandler) commandHandler(line);
    return false;
  }
  if (line[0] != '{') return false;

  JsonDocument doc;
  if (deserializeJson(doc, line)) return false;

  const char* type = doc["t"] | "res";
  if (strcmp(type, "res") == 0) return parseResources(doc);
  // Aquí irán otros tipos de paquete, por ejemplo "tok" para los tokens.
  return false;
}

bool statsPoll() {
  bool got = false;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        got |= handleLine(lineBuf);
        lineLen = 0;
      }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    } else {
      lineLen = 0;  // línea demasiado larga: se descarta
    }
  }
  return got;
}

bool statsConnected() { return stats.valid && millis() - stats.lastMs < DATA_TIMEOUT_MS; }
