#include "lora_lorawan.h"
#include "secrets.h"
#include <RadioLib.h>
#include <SPI.h>
#include <esp_mac.h>
#include <Preferences.h>

#ifdef USE_HX711
  #include <HX711.h>
  // TODO : définir les pins selon câblage sur la nouvelle carte
  // #define HX711_DOUT_PIN  XX
  // #define HX711_SCK_PIN   XX
  static HX711 scale;
  static bool  hx711Ok = false;
#endif

// ── RadioLib ─────────────────────────────────────────────────────────────────
static SX1262      radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY, SPI);
static LoRaWANNode node(&radio, &EU868);

static bool          _joined      = false;
static unsigned long _lastAlertMs = 0;
static uint16_t      _sendCount   = 0;  // compteur total envois (succès + échecs)
static Preferences   _prefs;

// ── Persistance DevNonce (NVS) ────────────────────────────────────────────────
// Flash ESP32 : 100 000 cycles/secteur, NVS wear-leveling sur 24 KB (6 secteurs)
// → ~750 000 écritures effectives. À 1 écriture/join (15 min) : ~21 ans.

static void noncesLoad()
{
  _prefs.begin("lorawan", true);  // lecture seule
  size_t len = _prefs.getBytesLength("nonces");
  if (len == RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
    uint8_t buf[RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
    _prefs.getBytes("nonces", buf, len);
    // DevNonce : octets [2-3] big-endian dans le buffer RadioLib
    uint16_t devNonce = ((uint16_t)buf[2] << 8) | buf[3];
    Serial.printf("[LoRa] Nonces NVS restaures — DevNonce : %u\n", devNonce);
    node.setBufferNonces(buf);
  } else {
    Serial.println("[LoRa] Pas de nonces en NVS (premier demarrage) — DevNonce : 0");
  }
  _prefs.end();
}

static void noncesSave()
{
  uint8_t* buf = node.getBufferNonces();
  _prefs.begin("lorawan", false);
  _prefs.putBytes("nonces", buf, RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  _prefs.end();
  Serial.println("[LoRa] Nonces sauvegardes en NVS");
}

// ── Cayenne LPP — encodage manuel ────────────────────────────────────────────
static uint8_t _buf[64];
static uint8_t _len;

static void lppReset() { _len = 0; }

static void lppTemp(uint8_t ch, float v)
{
  int16_t val = (int16_t)(v * 10.0f);
  _buf[_len++] = ch; _buf[_len++] = 0x67;
  _buf[_len++] = (val >> 8) & 0xFF; _buf[_len++] = val & 0xFF;
}

static void lppHum(uint8_t ch, float v)
{
  _buf[_len++] = ch; _buf[_len++] = 0x68;
  _buf[_len++] = (uint8_t)(v * 2.0f);
}

static void lppPres(uint8_t ch, float v)
{
  uint16_t val = (uint16_t)(v * 10.0f);
  _buf[_len++] = ch; _buf[_len++] = 0x73;
  _buf[_len++] = (val >> 8) & 0xFF; _buf[_len++] = val & 0xFF;
}

static void lppLux(uint8_t ch, float v)
{
  uint16_t val = (uint16_t)v;
  _buf[_len++] = ch; _buf[_len++] = 0x65;
  _buf[_len++] = (val >> 8) & 0xFF; _buf[_len++] = val & 0xFF;
}

// Analog input Cayenne LPP : résolution 0.01, signé, 2 octets
static void lppAnalog(uint8_t ch, float v)
{
  int16_t val = (int16_t)(v * 100.0f);
  _buf[_len++] = ch; _buf[_len++] = 0x02;
  _buf[_len++] = (val >> 8) & 0xFF; _buf[_len++] = val & 0xFF;
}

static void lppDigital(uint8_t ch, uint8_t v)
{
  _buf[_len++] = ch; _buf[_len++] = 0x00; _buf[_len++] = v;
}

// ── Lecture tension batterie ──────────────────────────────────────────────────
static float readBattV()
{
#if LORA_BATT_ADC_PIN >= 0
  uint32_t raw = analogRead(LORA_BATT_ADC_PIN);
  return (raw / 4095.0f) * 3.3f * LORA_BATT_RATIO;
#else
  return -1.0f;
#endif
}

// ── Lecture poids ─────────────────────────────────────────────────────────────
static float readWeightKg()
{
#ifdef USE_HX711
  if (!hx711Ok) return -1.0f;
  return scale.get_units(3) / 1000.0f;   // grammes → kg
#else
  return -1.0f;
#endif
}

// ── Construction payload Cayenne LPP ─────────────────────────────────────────
static void buildPayload(float temp, float hum, float pres, float lux,
                         float battV, float weightKg, float soundDb, bool alert)
{
  lppReset();
  if (temp     > -100.0f) lppTemp   (1, temp);
  if (hum      >=  0.0f)  lppHum    (2, hum);
  if (pres     >   0.0f)  lppPres   (3, pres);
  if (lux      >=  0.0f)  lppLux    (4, lux);
  if (battV    >=  0.0f)  lppAnalog (5, battV);
  if (weightKg >=  0.0f)  lppAnalog (6, weightKg);
  if (soundDb  >=  0.0f)  lppAnalog (7, soundDb / 100.0f);  // ex. 75 dB → 0.75
  lppDigital(8, alert ? 1 : 0);
}

// ── Envoi effectif ────────────────────────────────────────────────────────────
static bool transmit()
{
  _sendCount++;
  Serial.printf("[LoRa] Transmission %d octets Cayenne LPP (envoi #%u)...\n", _len, _sendCount);
  int16_t state = node.sendReceive(_buf, _len);
  bool ok = (state == RADIOLIB_ERR_NONE || state == RADIOLIB_LORAWAN_NO_DOWNLINK);
  if (ok)
    Serial.printf("[LoRa] Trame envoyee%s\n",
                  state == RADIOLIB_LORAWAN_NO_DOWNLINK ? " (pas de downlink)" : " + downlink recu");
  else
    Serial.printf("[LoRa] Erreur envoi : %d\n", state);
  return ok;
}

uint16_t loraGetSendCount() { return _sendCount; }

bool loraSendButtonEvent()
{
  if (!_joined) {
    Serial.println("[LoRa] Bouton : non joint, tentative join...");
    if (!loraRetryJoin()) return false;
  }
  // Payload Cayenne LPP : ch.9 analog = compteur (valeur / 100 sur dashboard)
  lppReset();
  lppAnalog(9, (float)_sendCount);  // _sendCount sera incrémenté dans transmit()
  Serial.printf("[LoRa] Bouton : envoi compteur = %u\n", _sendCount + 1);
  return transmit();
}

// ── Public ────────────────────────────────────────────────────────────────────

void loraInit()
{
  SPI.begin(LORA_SPI_SCK, LORA_SPI_MISO, LORA_SPI_MOSI, LORA_NSS);

  Serial.printf("SX1262 pins — NSS:%d DIO1:%d NRST:%d BUSY:%d SPI(SCK:%d MISO:%d MOSI:%d)\n",
                LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY,
                LORA_SPI_SCK, LORA_SPI_MISO, LORA_SPI_MOSI);
  // RF-SW : commutateur antenne Wio-SX1262 (actif haut en TX, bas en RX)
  radio.setRfSwitchPins(RADIOLIB_NC, LORA_RF_SW);

  // Re-init SPI hardware après avoir trouvé les bons pins
  SPI.begin(LORA_SPI_SCK, LORA_SPI_MISO, LORA_SPI_MOSI, LORA_NSS);

  Serial.print("SX1262 init... ");
  int16_t state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("erreur radio : %d\n", state);
    return;
  }
  Serial.println("OK");

  // DevEUI dérivé du MAC WiFi ESP32 (EUI-48 → EUI-64 selon IEEE 802)
  // Format : AA:BB:CC:FF:FE:DD:EE:FF (insertion FF:FE au milieu)
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  uint64_t devEUI = ((uint64_t)mac[0] << 56) | ((uint64_t)mac[1] << 48) |
                    ((uint64_t)mac[2] << 40) | ((uint64_t)0xFF   << 32) |
                    ((uint64_t)0xFE   << 24) | ((uint64_t)mac[3] << 16) |
                    ((uint64_t)mac[4] <<  8) | ((uint64_t)mac[5]);
  Serial.printf("DevEUI (MAC-derived) : %02X:%02X:%02X:FF:FE:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // AppKey = LORAWAN_APPKEY_PREFIX (11 octets) + mac[0..4] (5 octets)
  uint8_t appKey[16] = { LORAWAN_APPKEY_PREFIX, mac[0], mac[1], mac[2], mac[3], mac[4] };

  uint64_t joinEUI = LORAWAN_JOIN_EUI;

  Serial.printf("JoinEUI : %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                (uint8_t)(joinEUI >> 56), (uint8_t)(joinEUI >> 48),
                (uint8_t)(joinEUI >> 40), (uint8_t)(joinEUI >> 32),
                (uint8_t)(joinEUI >> 24), (uint8_t)(joinEUI >> 16),
                (uint8_t)(joinEUI >>  8), (uint8_t)(joinEUI));
  Serial.print("AppKey  : ");
  for (int i = 0; i < 16; i++) Serial.printf("%02X", appKey[i]);
  Serial.println();

  node.beginOTAA(joinEUI, devEUI, appKey, appKey);  // LoRaWAN 1.0.x : nwkKey = appKey
  noncesLoad();  // restaure DevNonce persisté (évite rejet Orange)

  { char ts[20] = "--:--:--"; struct tm ti;
    if (getLocalTime(&ti, 0)) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &ti);
    Serial.printf("[%s] LoRaWAN OTAA join Orange Live Objects... ", ts); }
  state = node.activateOTAA();
  noncesSave();  // sauvegarde DevNonce incrémenté (succès ou échec)
  if (state != RADIOLIB_LORAWAN_NEW_SESSION) {
    Serial.printf("join echoue : %d\n", state);
    return;
  }
  _joined = true;
  Serial.println("joint !");

  // Payload de confirmation post-join : 1 octet à 0x00
  Serial.println("[LoRa] Envoi payload de confirmation post-join...");
  uint8_t zero = 0x00;
  int16_t confState = node.sendReceive(&zero, 1);
  bool confOk = (confState == RADIOLIB_ERR_NONE || confState == RADIOLIB_LORAWAN_NO_DOWNLINK);
  Serial.printf("[LoRa] Confirmation : %s\n", confOk ? "OK" : "ECHEC");

#ifdef USE_HX711
  #if defined(HX711_DOUT_PIN) && defined(HX711_SCK_PIN)
    scale.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
    hx711Ok = scale.is_ready();
    Serial.println(hx711Ok ? "HX711 OK" : "HX711 non detecte");
  #else
    Serial.println("HX711 : pins non definis (voir lora_lorawan.h)");
  #endif
#endif
}

bool loraJoined() { return _joined; }

bool loraRetryJoin()
{
  if (_joined) return true;
  { char ts[20] = "--:--:--"; struct tm ti;
    if (getLocalTime(&ti, 0)) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &ti);
    Serial.printf("[%s] [LoRa] Retry join OTAA... ", ts); }
  int16_t state = node.activateOTAA();
  noncesSave();  // sauvegarde DevNonce incrémenté (succès ou échec)
  if (state != RADIOLIB_LORAWAN_NEW_SESSION) {
    Serial.printf("echec : %d\n", state);
    return false;
  }
  _joined = true;
  Serial.println("joint !");
  uint8_t zero = 0x00;
  int16_t confState = node.sendReceive(&zero, 1);
  bool confOk = (confState == RADIOLIB_ERR_NONE || confState == RADIOLIB_LORAWAN_NO_DOWNLINK);
  Serial.printf("[LoRa] Confirmation : %s\n", confOk ? "OK" : "ECHEC");
  return true;
}

bool loraSend(float temp, float hum, float pres, float lux,
              float battV, float weightKg, float soundDb, bool alert)
{
  if (!_joined) return false;

  // Compléter les valeurs non fournies avec les capteurs locaux
  if (battV    < 0.0f) battV    = readBattV();
  if (weightKg < 0.0f) weightKg = readWeightKg();

  buildPayload(temp, hum, pres, lux, battV, weightKg, soundDb, alert);
  return transmit();
}

bool loraSendAlert(float temp, float hum, float pres, float lux,
                   float battV, float weightKg, float soundDb)
{
  if (!_joined) return false;

  unsigned long now = millis();
  if (now - _lastAlertMs < LORA_ALERT_COOLDOWN_MS) {
    Serial.println("LoRa alerte : cooldown actif, ignoree");
    return false;
  }
  _lastAlertMs = now;

  if (battV    < 0.0f) battV    = readBattV();
  if (weightKg < 0.0f) weightKg = readWeightKg();

  buildPayload(temp, hum, pres, lux, battV, weightKg, soundDb, true);
  Serial.println("LoRa : envoi alerte");
  return transmit();
}
