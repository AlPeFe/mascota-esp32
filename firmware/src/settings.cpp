#include "settings.h"

#include <Preferences.h>

#include "config.h"

Settings settings;
static Preferences prefs;

void settingsLoad() {
  prefs.begin("mascota", true);
  settings.pet = prefs.getUChar("pet", 0);
  settings.brightness = prefs.getUChar("bright", 200);
  settings.invert = prefs.getBool("invert", DEFAULT_INVERT);
  settings.vertical = prefs.getBool("vertical", true);
  settings.flip = prefs.getBool("flip", false);
  prefs.end();
}

void settingsSave() {
  prefs.begin("mascota", false);
  prefs.putUChar("pet", settings.pet);
  prefs.putUChar("bright", settings.brightness);
  prefs.putBool("invert", settings.invert);
  prefs.putBool("vertical", settings.vertical);
  prefs.putBool("flip", settings.flip);
  prefs.end();
}
