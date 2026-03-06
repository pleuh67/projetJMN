#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid     = "Pleuh";
const char* password = "165116748221354";

WebServer server(80);

bool ledState = false;

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Controle LED</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      height: 100vh;
      margin: 0;
      background: #1a1a2e;
      color: #eee;
    }
    h1 { margin-bottom: 30px; }
    .led-indicator {
      width: 80px;
      height: 80px;
      border-radius: 50%;
      margin-bottom: 30px;
      transition: background 0.3s;
    }
    .led-on  { background: #ffdd57; box-shadow: 0 0 30px #ffdd57; }
    .led-off { background: #444; box-shadow: none; }
    .btn {
      padding: 15px 40px;
      margin: 10px;
      font-size: 1.2rem;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      transition: opacity 0.2s;
    }
    .btn:hover { opacity: 0.85; }
    .btn-on  { background: #ffdd57; color: #333; }
    .btn-off { background: #e74c3c; color: #fff; }
  </style>
</head>
<body>
  <h1>Controle LED</h1>
  <div class="led-indicator %LEDCLASS%"></div>
  <p>Etat : <strong>%LEDSTATE%</strong></p>
  <a href="/on"><button class="btn btn-on">Allumer</button></a>
  <a href="/off"><button class="btn btn-off">Eteindre</button></a>
</body>
</html>
)rawliteral";

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

  server.on("/",    handleRoot);
  server.on("/on",  handleOn);
  server.on("/off", handleOff);
  server.begin();
  Serial.println("Serveur HTTP demarre");
}

void loop()
{
  server.handleClient();
}
