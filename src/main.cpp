#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
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

// ── Page HTML ────────────────────────────────────────────────────────────────

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Station JMN</title>
  <style>
    * { box-sizing: border-box; }
    body {
      font-family: Arial, sans-serif;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      margin: 0;
      padding: 20px;
      background: #1a1a2e;
      color: #eee;
    }
    h1 { margin-bottom: 25px; }
    .led-indicator {
      width: 68px;
      height: 68px;
      border-radius: 50%;
      margin-bottom: 17px;
      transition: background 0.3s;
    }
    .led-on  { background: #ffdd57; box-shadow: 0 0 30px #ffdd57; }
    .led-off { background: #444; box-shadow: none; }
    .btn {
      padding: 12px 30px;
      margin: 6px;
      font-size: 1rem;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      transition: opacity 0.2s;
    }
    .btn:hover { opacity: 0.85; }
    .btn-on  { background: #ffdd57; color: #333; }
    .btn-off { background: #e74c3c; color: #fff; }
    h2 { margin: 25px 0 13px; font-size: 1rem; color: #aaa; letter-spacing: 1px; text-transform: uppercase; }
    .sensors {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 12px;
      width: 100%;
      max-width: 380px;
    }
    .card {
      background: #16213e;
      border-radius: 12px;
      padding: 15px 12px;
      text-align: center;
    }
    .card .label { font-size: 0.8rem; color: #888; margin-bottom: 4px; }
    .card .value { font-size: 1.55rem; font-weight: bold; color: #ffdd57; }
    .card .unit  { font-size: 0.85rem; color: #888; margin-top: 2px; }
    .update-time { margin-top: 12px; font-size: 0.75rem; color: #555; }
  </style>
</head>
<body>
  <h1>Station JMN</h1>

  <div class="led-indicator %LEDCLASS%"></div>
  <p>LED : <strong>%LEDSTATE%</strong></p>
  <a href="/on"><button  class="btn btn-on" >Allumer</button></a>
  <a href="/off"><button class="btn btn-off">Eteindre</button></a>

  <h2>Capteurs</h2>
  <div class="sensors">
    <div class="card">
      <div class="label">Température</div>
      <div class="value" id="temp">--</div>
      <div class="unit">°C</div>
    </div>
    <div class="card">
      <div class="label">Humidité</div>
      <div class="value" id="hum">--</div>
      <div class="unit">%</div>
    </div>
    <div class="card">
      <div class="label">Pression</div>
      <div class="value" id="pres">--</div>
      <div class="unit">hPa</div>
    </div>
    <div class="card">
      <div class="label">Luminosité</div>
      <div class="value" id="lux">--</div>
      <div class="unit">lux</div>
    </div>
  </div>
  <div class="update-time" id="upd">Chargement...</div>

  <script>
    function refresh() {
      fetch('/sensors')
        .then(r => r.json())
        .then(d => {
          document.getElementById('temp').textContent = d.temperature != null ? d.temperature.toFixed(1) : '--';
          document.getElementById('hum').textContent  = d.humidity    != null ? d.humidity.toFixed(1)    : '--';
          document.getElementById('pres').textContent = d.pressure    != null ? d.pressure.toFixed(1)    : '--';
          document.getElementById('lux').textContent  = d.lux         != null ? d.lux.toFixed(0)         : '--';
          document.getElementById('upd').textContent  = 'Mis à jour : ' + new Date().toLocaleTimeString();
        })
        .catch(() => {});
    }
    refresh();
    setInterval(refresh, 5000);
  </script>
</body>
</html>
)rawliteral";

// ── Handlers HTTP ─────────────────────────────────────────────────────────────

String buildPage()
{
  String page = htmlPage;
  if (ledState)
  {
    page.replace("%LEDCLASS%", "led-on");
    page.replace("%LEDSTATE%", "Allumee");
  }
  else
  {
    page.replace("%LEDCLASS%", "led-off");
    page.replace("%LEDSTATE%", "Eteinte");
  }
  return page;
}

void handleRoot()
{
  server.send(200, "text/html", buildPage());
}

void handleOn()
{
  ledState = true;
  digitalWrite(LED_BUILTIN, LOW);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleOff()
{
  ledState = false;
  digitalWrite(LED_BUILTIN, HIGH);
  server.sendHeader("Location", "/");
  server.send(303);
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

  server.on("/",        handleRoot);
  server.on("/on",      handleOn);
  server.on("/off",     handleOff);
  server.on("/sensors", handleSensors);
  server.begin();
  Serial.println("Serveur HTTP demarre");
}

void loop()
{
  server.handleClient();
}
