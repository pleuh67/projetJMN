#pragma once

#include <Arduino.h>

// Durée minimale entre deux SMS consécutifs (anti-spam).
// Modifier la valeur selon le besoin, par exemple :
//   5UL * 60UL * 1000UL  → 5 minutes
//  10UL * 60UL * 1000UL  → 10 minutes
#ifndef SMS_COOLDOWN_MS
#define SMS_COOLDOWN_MS (5UL * 60UL * 1000UL)
#endif

// Initialise le module (remet le compteur anti-spam à zéro).
// Appeler une fois dans setup(), après la connexion WiFi.
void smsInit();

// Envoie un SMS via l'API HTTP OVH.
// Retourne true si l'envoi est confirmé par l'API, false sinon.
// Les credentials sont lus depuis secrets.h (macros OVH_SMS_*).
bool sendSMS(const String& message);
