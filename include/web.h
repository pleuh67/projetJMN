#pragma once
#include <Arduino.h>

// Enregistre les routes HTTP et démarre le serveur web (port 80).
void webInit();

// À appeler dans loop() — traite les requêtes HTTP en attente.
void webHandle();
