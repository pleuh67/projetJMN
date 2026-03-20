#include "web.h"
#include "mesures.h"
#include <WebServer.h>
#include <SPIFFS.h>
#include <time.h>

static WebServer server(80);
static bool      ledState = false;

// ── Helpers SPIFFS ────────────────────────────────────────────────────────────

static void serveFile(const char* path, const char* contentType)
{
  File f = SPIFFS.open(path, "r");
  if (!f) { server.send(404, "text/plain", "File not found"); return; }
  server.streamFile(f, contentType);
  f.close();
}

// ── Handlers statiques ────────────────────────────────────────────────────────

static void handleRoot()   { serveFile("/index.html", "text/html"); }
static void handleCSS()    { serveFile("/style.css",  "text/css"); }
static void handleW3CSS()  { serveFile("/w3.min.css", "text/css"); }
static void handleJS()     { serveFile("/app.js",     "application/javascript"); }

static void handleState()
{
  server.send(200, "application/json", ledState ? "{\"led\":true}" : "{\"led\":false}");
}

static void handleOn()
{
  ledState = true;
  digitalWrite(LED_BUILTIN, LOW);
  Serial.print("LED ON  — requete de : ");
  Serial.println(server.client().remoteIP());
  server.send(200, "application/json", "{\"led\":true}");
}

static void handleOff()
{
  ledState = false;
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.print("LED OFF — requete de : ");
  Serial.println(server.client().remoteIP());
  server.send(200, "application/json", "{\"led\":false}");
}

static void handleTime()
{
  struct tm timeinfo;
  char buf[32];
  if (!getLocalTime(&timeinfo, 0)) {
    server.send(503, "application/json", "{\"datetime\":null}");
    return;
  }
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  server.send(200, "application/json", String("{\"datetime\":\"") + buf + "\"}");
}

static void handleSensors()
{
  float db = readSoundDb();

  String json = "{";
  json += (db >= 0.0f) ? "\"sound_db\":" + String(db, 1)
                       : "\"sound_db\":null";
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ── Interface publique ────────────────────────────────────────────────────────

void webInit()
{
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

void webHandle()
{
  server.handleClient();
}
