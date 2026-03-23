#pragma once
#include <Arduino.h>

// ── Fonctionnalités optionnelles ─────────────────────────────────────────────
// Décommenter dans platformio.ini (build_flags) selon le matériel connecté :
//   -DUSE_INMP441   son    — conflit GPIO1-3 avec Wio-SX1262 sur XIAO ESP32S3
//   -DUSE_HX711     poids  — pins libres requises, prévu pour ESP32S3 Dev Kit

// ── Pins Wio-SX1262 pour XIAO ESP32S3 ────────────────────────────────────────
#define LORA_NSS   D4   // GPIO5  (NSS/CS)  schema 4
#define LORA_DIO1  D1   // GPIO2  (DIO1/IRQ)
#define LORA_NRST  D2   // GPIO3  (RESET)
#define LORA_BUSY  D3   // GPIO4  (BUSY)    schema 6
#define LORA_RF_SW D5   // GPIO6  (RF-SW antenne) — SCL déplacé sur GPIO7  schema 7
// SPI : SCK=GPIO9(D9), MISO=GPIO8(D8), MOSI=GPIO10(D10)
#define LORA_SPI_SCK    D8 //7
#define LORA_SPI_MISO   D9 //8
#define LORA_SPI_MOSI   D10 //9

// ── Paramètres réseau ─────────────────────────────────────────────────────────
#define LORA_SEND_INTERVAL_MS   (5UL * 60UL * 1000UL)  // envoi périodique : 5 min
#define LORA_ALERT_COOLDOWN_MS  (5UL * 60UL * 1000UL)  // anti-spam alertes : 5 min
#define LORA_MAX_SEND_FAILURES  3                       // échecs consécutifs → re-join

// ── Pin ADC tension batterie ──────────────────────────────────────────────────
// Brancher un pont diviseur (ex. 100k/100k) entre VBAT et GND,
// point milieu sur LORA_BATT_ADC_PIN. Ajuster LORA_BATT_RATIO selon les résistances.
// Mettre -1 pour désactiver la mesure.
#define LORA_BATT_ADC_PIN   -1       // TODO : affecter selon câblage
#define LORA_BATT_RATIO     2.0f     // (R1+R2)/R2

// ── Canaux Cayenne LPP ────────────────────────────────────────────────────────
// Compatible Orange Live Objects sans décodeur custom.
// Ch.1 Température  Ch.2 Humidité   Ch.3 Pression   Ch.4 Luminosité
// Ch.5 Batterie(V)  Ch.6 Poids(kg)  Ch.7 Son(dB/100) Ch.8 Alerte

// ── Interface publique ────────────────────────────────────────────────────────

// Initialise le module SX1262 et effectue le join OTAA Orange Live Objects.
// À appeler dans setup() après WiFi (NTP déjà synchronisé n'est pas requis).
void loraInit();

// Retourne true si le join OTAA a réussi.
bool loraJoined();

// Retente le join OTAA si non connecté. Retourne true si joint.
bool loraRetryJoin();

// Retourne le compteur total d'envois (succès + échecs).
uint16_t loraGetSendCount();

// Envoi déclenché par bouton : payload Cayenne LPP ch.9 = compteur envois.
bool loraSendButtonEvent();

// Envoie une trame périodique avec tous les capteurs.
// Passer -1 (ou -200 pour temp) pour les valeurs non disponibles → omises du payload.
bool loraSend(float temp, float hum, float pres, float lux,
              float battV, float weightKg, float soundDb, bool alert = false);

// Envoi immédiat d'une alerte (flag alerte=1, autres champs actuels).
// Respecte LORA_ALERT_COOLDOWN_MS. Retourne false si cooldown actif.
bool loraSendAlert(float temp, float hum, float pres, float lux,
                   float battV, float weightKg, float soundDb);
