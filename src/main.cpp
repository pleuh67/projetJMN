#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include "secrets.h"

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WebServer server(80);

bool ledState = false;

Adafruit_BME280 bme;
BH1750          lightMeter;

bool bmeOk = false;
bool bhOk  = false;

// ── Helpers SPIFFS ─────────────────────────────────────────────────────────────

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
  server.send(200, "application/json", "{\"led\":true}");
}

void handleOff()
{
  ledState = false;
  digitalWrite(LED_BUILTIN, HIGH);
  server.send(200, "application/json", "{\"led\":false}");
}

void handleSensors()
{
  String json = "{";

  if (bmeOk)
  {
    json += "\"temperature\":"  + String(bme.readTemperature(), 2) + ",";
    json += "\"humidity\":"     + String(bme.readHumidity(),    2) + ",";
    json += "\"pressure\":"     + String(bme.readPressure() / 100.0F, 2) + ",";
  }
  else
  {
    json += "\"temperature\":null,\"humidity\":null,\"pressure\":null,";
  }

  if (bhOk)
    json += "\"lux\":" + String(lightMeter.readLightLevel(), 2);
  else
    json += "\"lux\":null";

  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  while (!Serial)
  {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(500);
  }
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("Port série OK");

  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS mount failed");
    return;
  }
  Serial.println("SPIFFS OK");

  // I2C — SDA=5, SCL=6 (XIAO ESP32S3)
  Wire.begin(5, 6);

  bmeOk = bme.begin(0x76);
  if (!bmeOk) bmeOk = bme.begin(0x77);  // adresse alternative
  Serial.println(bmeOk ? "BME280 OK" : "BME280 non detecte");

  bhOk = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println(bhOk ? "BH1750 OK" : "BH1750 non detecte");

  WiFi.begin(ssid, password);
  Serial.print("Connexion WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connecte ! IP : ");
  Serial.println(WiFi.localIP());

  server.on("/",           handleRoot);
  server.on("/style.css",  handleCSS);
  server.on("/w3.min.css", handleW3CSS);
  server.on("/app.js",     handleJS);
  server.on("/state",     handleState);
  server.on("/on",        handleOn);
  server.on("/off",       handleOff);
  server.on("/sensors",   handleSensors);
  server.begin();
  Serial.println("Serveur HTTP demarre");
}

void loop()
{
  server.handleClient();
}
