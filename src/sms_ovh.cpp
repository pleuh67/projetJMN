#include "sms_ovh.h"
#include "secrets.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// Credentials attendus dans secrets.h :
//   #define OVH_SMS_ACCOUNT  "sms-XXXXXXX"
//   #define OVH_SMS_LOGIN    "XXXXXXX"
//   #define OVH_SMS_PASSWORD "XXXXXXX"
//   #define OVH_SMS_FROM     "XXXXXXX"       // expéditeur (nom ou numéro)
//   #define OVH_SMS_TO       "+336XXXXXXXX"  // destinataire au format international

// ── Anti-spam ─────────────────────────────────────────────────────────────────

static unsigned long lastSmsSentAt = 0;  // 0 = jamais envoyé

// ── Encodage URL (RFC 3986, ASCII uniquement) ─────────────────────────────────

static String urlEncode(const String& s)
{
  String encoded;
  encoded.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); i++)
  {
    char c = s[i];
    if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~')
      encoded += c;
    else
    {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

// ── API publique ───────────────────────────────────────────────────────────────

void smsInit()
{
  lastSmsSentAt = 0;
  Serial.println("[SMS] Module OVH SMS initialise");
}

bool sendSMS(const String& message)
{
  // 1. WiFi connecté ?
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[SMS] Erreur : WiFi non connecte");
    return false;
  }

  // 2. Anti-spam
  unsigned long now = millis();
  if (lastSmsSentAt != 0 && (now - lastSmsSentAt) < SMS_COOLDOWN_MS)
  {
    unsigned long remainSec = (SMS_COOLDOWN_MS - (now - lastSmsSentAt)) / 1000UL;
    Serial.print("[SMS] Anti-spam : attendre encore ");
    Serial.print(remainSec);
    Serial.println(" s avant le prochain SMS");
    return false;
  }

  // 3. Construction de l'URL GET
  String url = "https://www.ovh.com/cgi-bin/sms/http2sms.cgi";
  url += "?account="     + urlEncode(OVH_SMS_ACCOUNT);
  url += "&login="       + urlEncode(OVH_SMS_LOGIN);
  url += "&password="    + urlEncode(OVH_SMS_PASSWORD);
  url += "&from="        + urlEncode(OVH_SMS_FROM);
  url += "&to="          + urlEncode(OVH_SMS_TO);
  url += "&message="     + urlEncode(message);
  url += "&noStop=1";                // pas de mention STOP (usage pro)
  url += "&contentType=text%2Fjson"; // réponse JSON

  Serial.print("[SMS] Envoi : ");
  Serial.println(message);

  // 4. Requête HTTPS
  WiFiClientSecure wifiClient;
  // setInsecure() accepte n'importe quel certificat — suffisant pour ce cas d'usage.
  // Pour renforcer la sécurité en production, utiliser setCACert(ovh_root_ca).
  wifiClient.setInsecure();

  HTTPClient http;
  http.setTimeout(10000);  // timeout 10 s

  if (!http.begin(wifiClient, url))
  {
    Serial.println("[SMS] Erreur : initialisation HTTPClient echouee");
    return false;
  }

  int    httpCode = http.GET();
  String response = http.getString();
  http.end();

  Serial.print("[SMS] Code HTTP : ");
  Serial.println(httpCode);
  Serial.print("[SMS] Reponse   : ");
  Serial.println(response);

  // 5. Analyse de la réponse
  //    OVH renvoie {"status":100} en cas de succès (code 100 = envoi accepté).
  if (httpCode != 200)
  {
    Serial.print("[SMS] Erreur HTTP inattendue : ");
    Serial.println(httpCode);
    return false;
  }

  bool ok = (response.indexOf("\"status\":100")    >= 0 ||
             response.indexOf("\"status\": 100")   >= 0);

  if (ok)
  {
    lastSmsSentAt = now;
    Serial.println("[SMS] SMS envoye avec succes");
  }
  else
  {
    Serial.println("[SMS] Echec API OVH (voir reponse ci-dessus)");
  }

  return ok;
}
