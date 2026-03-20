#pragma once
#include <Arduino.h>

// Seuil déclenchement alerte sonore
#define SOUND_ALERT_DB  75.0f

// Initialise le bus I2S pour l'INMP441 (si -DUSE_INMP441 activé).
void mesuresInit();

// Niveau sonore en dB via INMP441 (nécessite -DUSE_INMP441).
// Retourne -1 si non disponible.
float readSoundDb();
