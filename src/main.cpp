#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <math.h>
#include <time.h>
#include <RTClib.h>
#include "secrets.h"
#include "sms_ovh.h"
#include "lora_lorawan.h"

#ifdef USE_INMP441
  #include <driver/i2s.h>
#endif

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WebServer server(80);

bool ledState = false;

Adafruit_BME280 bme;
BH1750          lightMeter;

bool bmeOk = false;
bool bhOk  = false;
bool rtcOk = false;

RTC_DS3231 rtc;

// ── Seuil alerte sonore ───────────────────────────────────────────────────────
#define SOUND_ALERT_DB  75.0f

// ── I2S / INMP441 — activé uniquement si USE_INMP441 défini ──────────────────
// ⚠️ Conflit GPIO1-3 avec Wio-SX1262 sur XIAO ESP32S3.
//    Décommenter -DUSE_INMP441 dans platformio.ini uniquement sur carte avec + d'IO.

#ifdef USE_INMP441

#define I2S_PORT        I2S_NUM_0
#define I2S_PIN_BCLK    1     // D0
#define I2S_PIN_WS      2     // D1
#define I2S_PIN_DATA    3     // D2
#define I2S_SAMPLE_RATE 16000
#define I2S_READ_LEN    512

static bool i2sOk = false;

static void i2sInit()
{
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = I2S_SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = 256,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };
  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_PIN_BCLK,
    .ws_io_num    = I2S_PIN_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_PIN_DATA
  };
  if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("INMP441 : driver I2S non installe"); return;
  }
  if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
    Serial.println("INMP441 : configuration des pins echouee"); return;
  }
  i2sOk = true;
  Serial.println("INMP441 (I2S) OK");
}

static float readSoundDb()
{
  if (!i2sOk) return -1.0f;
  int32_t samples[I2S_READ_LEN];
  size_t  bytesRead = 0;
  i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(100));
  int count = (int)(bytesRead / sizeof(int32_t));
  if (count == 0) return -1.0f;
  double sumSq = 0.0;
  for (int i = 0; i < count; i++) {
    float s = (float)(samples[i] >> 8) / 8388607.0f;
    sumSq += (double)s * s;
  }
  double rms = sqrt(sumSq / count);
  if (rms < 1e-10) return 0.0f;
  float db = 20.0f * (float)log10(rms) + 120.0f;
  return (db < 0.0f) ? 0.0f : db;
}

#else

static float readSoundDb() { return -1.0f; }

#endif  // USE_INMP441

// ── Helpers SPIFFS ────────────────────────────────────────────────────────────

void serveFile(const char* path, const char* contentType)
{
  File f = SPIFFS.open(path, "r");
  if (!f) { server.send(404, "text/plain", "File not found"); return; }
  server.streamFile(f, contentType);
  f.close();
}

// ── Handlers HTTP ─────────────────────────────────────────────────────────────

void handleRoot()    { serveFile("/index.html",  "text/html"); }
void handleCSS()     { serveFile("/style.css",   "text/css"); }
void handleW3CSS()   { serveFile("/w3.min.css",  "text/css"); }
void handleJS()      { serveFile("/app.js",      "application/javascript"); }

void handleState()
{
  server.send(200, "application/json", ledState ? "{\"led\":true}" : "{\"led\":false}");
}

void handleOn()
{
  ledState = true;
  digitalWrite(LED_BUILTIN, LOW);
  Serial.print("LED ON  — requete de : ");
  Serial.println(server.client().remoteIP());
  server.send(200, "application/json", "{\"led\":true}");
}

void handleOff()
{
  ledState = false;
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.print("LED OFF — requete de : ");
  Serial.println(server.client().remoteIP());
  server.send(200, "application/json", "{\"led\":false}");
}

void handleTime()
{
  char buf[32];
  if (rtcOk) {
    DateTime now = rtc.now();
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
  } else {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0)) {
      server.send(503, "application/json", "{\"datetime\":null}");
      return;
    }
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  }
  server.send(200, "application/json", String("{\"datetime\":\"") + buf + "\"}");
}

void handleSensors()
{
  String json = "{";

  if (bmeOk) {
    json += "\"temperature\":"  + String(bme.readTemperature(), 2) + ",";
    json += "\"humidity\":"     + String(bme.readHumidity(),    2) + ",";
    json += "\"pressure\":"     + String(bme.readPressure() / 100.0F, 2) + ",";
  } else {
    json += "\"temperature\":null,\"humidity\":null,\"pressure\":null,";
  }

  if (bhOk)
    json += "\"lux\":" + String(lightMeter.readLightLevel(), 2) + ",";
  else
    json += "\"lux\":null,";

  float db = readSoundDb();
  if (db >= 0.0f)
    json += "\"sound_db\":" + String(db, 1);
  else
    json += "\"sound_db\":null";

  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ── Lecture capteurs courants ─────────────────────────────────────────────────
static void getSensorValues(float &temp, float &hum, float &pres, float &lux, float &db)
{
  temp = bmeOk ? bme.readTemperature()         : -200.0f;
  hum  = bmeOk ? bme.readHumidity()            :   -1.0f;
  pres = bmeOk ? bme.readPressure() / 100.0f   :   -1.0f;
  lux  = bhOk  ? lightMeter.readLightLevel()   :   -1.0f;
  db   = readSoundDb();
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(1, INPUT_PULLUP);  // bouton sur D0/GPIO1, actif bas
  for (unsigned long t = millis(); millis() - t < 10000;) {
    digitalWrite(LED_BUILTIN, LOW);   // allumée (active-bas)
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);  // éteinte
    delay(250);
  }
  digitalWrite(LED_BUILTIN, LOW);   // allumée pendant le démarrage
  Serial.begin(115200);
  Serial.println("Port serie OK");

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

  // I2C sur GPIO43/GPIO44 (D6/D7) — GPIO5(D4) et GPIO6(D5) réservés Wio-SX1262 (NSS/RF-SW)
  // Note : capteurs I2C à brancher sur D6(SDA) et D7(SCL) quand Wio-SX1262 est connecté
  Wire.begin(43, 44);


  rtcOk = rtc.begin();
  Serial.println(rtcOk ? "DS3231 OK" : "DS3231 non detecte");

  bmeOk = bme.begin(0x76);
  if (!bmeOk) bmeOk = bme.begin(0x77);
  Serial.println(bmeOk ? "BME280 OK" : "BME280 non detecte");

  bhOk = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println(bhOk ? "BH1750 OK" : "BH1750 non detecte");

#ifdef USE_INMP441
  i2sInit();
#else
  Serial.println("INMP441 : desactive (USE_INMP441 non defini)");
#endif

  WiFi.begin(ssid, password);
  Serial.printf("Connexion WiFi au reseau : %s\n", ssid);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nConnecte ! SSID : %s | IP : %s\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
  struct tm timeinfo;
  if (rtcOk && getLocalTime(&timeinfo, 10000)) {
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    Serial.println("RTC synchronisee avec NTP");
  } else {
    Serial.println("Sync NTP echouee ou RTC absente");
  }

  smsInit();
  loraInit();

  server.on("/",           handleRoot);
  server.on("/style.css",  handleCSS);
  server.on("/w3.min.css", handleW3CSS);
  server.on("/app.js",     handleJS);
  server.on("/state",      handleState);
  server.on("/on",         handleOn);
  server.on("/off",        handleOff);
  server.on("/sensors",    handleSensors);
  server.on("/time",       handleTime);
  server.begin();
  Serial.println("Serveur HTTP demarre");
}

void loop()
{
  server.handleClient();

  static unsigned long lastLoraSend   = 0;
  static unsigned long lastAlertCheck = 0;
  static bool          btnLastState   = HIGH;
  static unsigned long btnLastChange  = 0;
  unsigned long now = millis();

  // ── Bouton D0/GPIO1 — envoi compteur sur pression (debounce 50 ms) ──────────
  bool btnState = digitalRead(1);
  if (btnState != btnLastState && now - btnLastChange > 50) {
    btnLastChange = now;
    btnLastState  = btnState;
    if (btnState == LOW) {   // front descendant = appui
      Serial.println("[BTN] Appui detecte");
      loraSendButtonEvent();
    }
  }

  // ── Envoi périodique LoRa (ou retry join si non connecté) ─────────────────
  if (now - lastLoraSend >= LORA_SEND_INTERVAL_MS) {
    lastLoraSend = now;
    if (!loraJoined()) {
      loraRetryJoin();
    } else {
      float temp, hum, pres, lux, db;
      getSensorValues(temp, hum, pres, lux, db);
      Serial.printf("[LoRa] Envoi — T:%.1f°C H:%.1f%% P:%.0fhPa L:%.0flux dB:%.1f\n",
                    temp, hum, pres, lux, db);
      bool ok = loraSend(temp, hum, pres, lux, -1.0f, -1.0f, db, false);
      Serial.printf("[LoRa] Resultat : %s\n", ok ? "OK" : "ECHEC");
    }
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
