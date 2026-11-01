#pragma once

// Funciones que main.cpp ofrece a las pantallas.
class Pet;

#define PAGE_COUNT 4

Pet* appPet();
void appSetPet(int index);
void appApplyDisplay();  // aplica brillo, inversión y giro de los ajustes
int appPage();
