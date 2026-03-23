#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <time.h>
#include "secrets.h"
#include "sms_ovh.h"
#include "lora_lorawan.h"
#include "web.h"
#include "mesures.h"

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// ── Lecture capteurs courants ─────────────────────────────────────────────────
static void getSensorValues(float &temp, float &hum, float &pres, float &lux, float &db)
{
  temp = -200.0f;
  hum  =   -1.0f;
  pres =   -1.0f;
  lux  =   -1.0f;
  db   = readSoundDb();
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(1, INPUT_PULLUP);  // bouton sur D0/GPIO1, actif bas
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_BUILTIN, LOW);   // flash 250 ms
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);  // éteinte 750 ms
    delay(750);
  }
  digitalWrite(LED_BUILTIN, LOW);   // allumée pendant le démarrage
  Serial.begin(115200);
  Serial.println("Port serie OK");
  Serial.printf("Build : %s %s\n", __DATE__, __TIME__);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed"); return;
  }
  Serial.println("SPIFFS OK");

  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.printf("  [SPIFFS] %s  (%u octets)\n", file.name(), file.size());
    file = root.openNextFile();
  }

  mesuresInit();

  WiFi.begin(ssid, password);
  Serial.printf("Connexion WiFi au reseau : %s\n", ssid);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nConnecte ! SSID : %s | IP : %s\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.printf("NTP synchronise : %s\n", buf);
  } else {
    Serial.println("Sync NTP echouee");
  }

  smsInit();
  digitalWrite(LED_BUILTIN, LOW);   // LED allumée pendant le join
  loraInit();
  digitalWrite(LED_BUILTIN, HIGH);  // LED éteinte, heartbeat prend le relai

  webInit();
}

void loop()
{
  webHandle();

  static unsigned long lastLoraSend   = 0;
  static unsigned long lastAlertCheck = 0;
  static bool          btnLastState   = HIGH;
  static unsigned long btnLastChange  = 0;
  static unsigned long lastFlash      = 0;
  static bool          flashOn        = false;
  unsigned long now = millis();

  // ── Heartbeat LED : flash 250 ms toutes les 2 s ───────────────────────────
  if (!flashOn && now - lastFlash >= 2000UL) {
    lastFlash = now;
    flashOn   = true;
    digitalWrite(LED_BUILTIN, LOW);   // allumée
  }
  if (flashOn && now - lastFlash >= 250UL) {
    flashOn = false;
    digitalWrite(LED_BUILTIN, HIGH);  // éteinte
  }

  // ── Bouton D0/GPIO1 — envoi compteur sur pression (debounce 50 ms) ──────────
  bool btnState = digitalRead(1);
  if (btnState != btnLastState && now - btnLastChange > 50) {
    btnLastChange = now;
    btnLastState  = btnState;
    if (btnState == LOW) {   // front descendant = appui
      Serial.println("[BTN] Appui detecte");
      digitalWrite(LED_BUILTIN, LOW);
      loraSendButtonEvent();
      digitalWrite(LED_BUILTIN, HIGH);
      lastFlash = millis();  // évite un flash heartbeat immédiat après
    }
  }

  // ── Envoi périodique LoRa (ou retry join si non connecté) ─────────────────
  if (now - lastLoraSend >= LORA_SEND_INTERVAL_MS) {
    lastLoraSend = now;
    digitalWrite(LED_BUILTIN, LOW);
    if (!loraJoined()) {
      loraRetryJoin();
    } else {
      float temp, hum, pres, lux, db;
      getSensorValues(temp, hum, pres, lux, db);
      char ts[20] = "--:--:--";
      struct tm tinfo;
      if (getLocalTime(&tinfo, 0)) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tinfo);
      Serial.printf("[LoRa] %s — T:%.1f°C H:%.1f%% P:%.0fhPa L:%.0flux dB:%.1f\n",
                    ts, temp, hum, pres, lux, db);
      bool ok = loraSend(temp, hum, pres, lux, -1.0f, -1.0f, db, false);
      Serial.printf("[LoRa] Resultat : %s\n", ok ? "OK" : "ECHEC");
    }
    digitalWrite(LED_BUILTIN, HIGH);
    lastFlash = millis();  // évite un flash heartbeat immédiat après
  }

  // ── Détection alerte sonore (toutes les 10 s) ─────────────────────────────
#ifdef USE_INMP441
  if (now - lastAlertCheck >= 10000UL) {
    lastAlertCheck = now;
    float db = readSoundDb();
    if (db >= SOUND_ALERT_DB) {
      sendSMS("Alerte bruit : " + String(db, 0) + " dB");
      if (loraJoined()) {
        float temp, hum, pres, lux, dbFull;
        getSensorValues(temp, hum, pres, lux, dbFull);
        loraSendAlert(temp, hum, pres, lux, -1.0f, -1.0f, dbFull);
      }
    }
  }
#endif
}
