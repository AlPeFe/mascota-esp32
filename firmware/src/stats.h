#pragma once
#include <Arduino.h>
#include "config.h"

// Recursos del PC tal y como los envía el monitor (una línea JSON por segundo).
struct PcStats {
  bool valid = false;     // ha llegado al menos un paquete
  uint32_t lastMs = 0;    // millis() del último paquete

  float cpu = 0;          // % total
  float cpuSmooth = 0;    // % suavizado (para que la mascota no cambie de humor a saltos)
  uint8_t cores[32] = {};
  uint8_t coreCount = 0;

  float ram = 0, ramUsed = 0, ramTotal = 0;  // %, GB, GB

  float gpu = -1;                             // % (-1 = no disponible)
  float vram = -1, vramUsed = 0, vramTotal = 0;
  float gpuTemp = -1;                         // ºC
  char gpuName[24] = "";

  float disk = 0;                     // % ocupado del disco del sistema
  float diskRead = 0, diskWrite = 0;  // KB/s
  float netDown = 0, netUp = 0;       // KB/s

  int hour = -1, minute = -1;

  int idle = -1;  // segundos sin tocar teclado ni ratón (-1 = no se sabe)

  float cpuHist[HISTORY_LEN] = {};
  float netHist[HISTORY_LEN] = {};
  int histHead = 0;  // posición donde irá la siguiente muestra
};

extern PcStats stats;

void statsBegin();
bool statsPoll();  // lee el puerto serie; true si ha llegado un paquete nuevo
bool statsConnected();

// Las líneas que empiezan por "!" son órdenes de depuración (ver main.cpp).
void statsSetCommandHandler(void (*handler)(const char*));
