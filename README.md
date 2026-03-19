# ProjetJMN — Station de mesure connectée

Station de mesure environnementale basée sur **XIAO ESP32S3** avec interface web, alertes SMS OVH et transmission **LoRaWAN vers Orange Live Objects**.

Repo GitHub : <https://github.com/pleuh67/projetJMN>

---

## Matériel

### Carte principale

| Composant | Détail |
|-----------|--------|
| Microcontrôleur | Seeed Studio XIAO ESP32S3 |
| I2C | SDA = GPIO5, SCL = GPIO6 |
| LED intégrée | Active-bas (LOW = allumée, HIGH = éteinte) |
| USB CDC | `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1` |

### Capteurs I2C (bus partagé SDA/SCL)

| Capteur | Mesure | Adresse I2C |
|---------|--------|-------------|
| BME280 | Température (°C), Humidité (%), Pression (hPa) | 0x76 ou 0x77 |
| BH1750 | Luminosité (lux) | 0x23 |
| DS3231 | Horloge temps réel | 0x68 |

### Module LoRa

| Composant | Détail |
|-----------|--------|
| Module | Seeed Wio-SX1262 for XIAO |
| Puce | SX1262 |
| Réseau | LoRaWAN EU868 — Orange Live Objects (OTAA) |
| Bus SPI | SCK=GPIO7(D8), MISO=GPIO8(D9), MOSI=GPIO9(D10) |
| Contrôle | NSS=GPIO2(D1), DIO1=GPIO1(D0), NRST=GPIO3(D2), BUSY=GPIO4(D3) |

### Capteurs optionnels (compilation conditionnelle)

| Capteur | Mesure | Flag | Statut |
|---------|--------|------|--------|
| INMP441 | Niveau sonore (dB SPL) | `-DUSE_INMP441` | Réservé — conflit GPIO1-3 avec Wio-SX1262 sur XIAO ESP32S3 |
| HX711 + cellule | Poids (kg) | `-DUSE_HX711` | Réservé — pins libres requises (prévu ESP32S3 Dev Kit) |

> **Note migration :** Le passage à un **ESP32S3 Dev Kit C1** permettra d'utiliser simultanément le LoRa, l'INMP441 et le HX711 grâce au plus grand nombre de GPIO disponibles.

---

## Architecture

```
ProjetJMN/
├── src/
│   ├── main.cpp           — serveur HTTP, capteurs, boucle principale
│   ├── sms_ovh.cpp        — module SMS OVH (implémentation)
│   └── lora_lorawan.cpp   — module LoRaWAN SX1262 / RadioLib (implémentation)
├── include/
│   ├── secrets.h          — credentials WiFi + OVH + LoRaWAN (gitignorée)
│   ├── secrets.h.example  — template vide (commitée)
│   ├── sms_ovh.h          — interface module SMS OVH
│   └── lora_lorawan.h     — interface module LoRaWAN
├── data/                  — fichiers SPIFFS (servis par le serveur HTTP)
│   ├── index.html         — page principale
│   ├── w3.min.css         — W3.CSS 4.15 minifié (~22 KB)
│   ├── style.css          — overrides thème sombre
│   └── app.js             — logique JS LED + capteurs
├── docs/
│   └── guide-jean-marc.md — documentation utilisateur final
├── platformio.ini
└── README.md
```

---

## Configuration

### 1. Credentials (`include/secrets.h`)

Copier `secrets.h.example` en `secrets.h` et renseigner :

```cpp
// WiFi
#define WIFI_SSID     "votre_ssid"
#define WIFI_PASSWORD "votre_mot_de_passe"

// OVH SMS
#define OVH_SMS_ACCOUNT  "sms-XXXXXXX"
#define OVH_SMS_LOGIN    "XXXXXXX"
#define OVH_SMS_PASSWORD "XXXXXXX"
#define OVH_SMS_FROM     "XXXXXXX"
#define OVH_SMS_TO       "+336XXXXXXXX"

// LoRaWAN — Orange Live Objects
// Live Objects > Devices > votre device > LoRa
// Le DevEUI est dérivé automatiquement du MAC WiFi ESP32 (affiché au démarrage série)
#define LORAWAN_JOIN_EUI  0x...ULL      // JoinEUI / AppEUI (8 octets)
#define LORAWAN_APP_KEY   { 0x..., ... } // AppKey (16 octets)
```

### 2. Fonctionnalités optionnelles (`platformio.ini`)

Décommenter dans `build_flags` selon le matériel connecté :

```ini
build_flags =
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1
  ; -DUSE_INMP441   ; son — activer sur carte avec GPIO libres
  ; -DUSE_HX711     ; poids — activer sur carte avec GPIO libres
```

### 3. Tension batterie (`include/lora_lorawan.h`)

```cpp
#define LORA_BATT_ADC_PIN   -1    // mettre le n° GPIO du point milieu du diviseur
#define LORA_BATT_RATIO     2.0f  // (R1+R2)/R2 — ex. 2.0 pour 100k/100k
```

Câblage suggéré : pont diviseur R1=100kΩ / R2=100kΩ entre VBAT et GND, point milieu sur `LORA_BATT_ADC_PIN`.

---

## Fonctionnalités

### Serveur HTTP (port 80)

| Endpoint | Méthode | Description | Réponse |
|----------|---------|-------------|---------|
| `/` | GET | Interface web | HTML |
| `/sensors` | GET | Valeurs capteurs | JSON |
| `/state` | GET | État LED | JSON |
| `/time` | GET | Date/heure RTC | JSON |
| `/on` | GET | Allume LED | JSON |
| `/off` | GET | Éteint LED | JSON |

Exemple `/sensors` :
```json
{
  "temperature": 22.54,
  "humidity": 58.20,
  "pressure": 1013.25,
  "lux": 320.0,
  "sound_db": 68.3
}
```

### SMS OVH

- API : `https://www.ovh.com/cgi-bin/sms/http2sms.cgi` (GET HTTPS)
- Anti-spam : cooldown 5 min (`SMS_COOLDOWN_MS`, surchargeable dans `platformio.ini`)
- HTTPS via `WiFiClientSecure` avec `setInsecure()`
- Déclenchement : niveau sonore ≥ 75 dB (si `USE_INMP441` actif)

### LoRaWAN — Orange Live Objects

- Protocole : LoRaWAN EU868, activation OTAA
- Bibliothèque : RadioLib 6.6+
- Envoi périodique : toutes les 5 min (`LORA_SEND_INTERVAL_MS`)
- Envoi alerte : `loraSendAlert()` avec anti-spam 5 min (`LORA_ALERT_COOLDOWN_MS`)

#### Payload Cayenne LPP

Format nativement décodé par Orange Live Objects (pas de décodeur custom requis).

> **DevEUI** : dérivé automatiquement du MAC WiFi ESP32 (EUI-48 → EUI-64 IEEE 802, insertion `FF:FE`). Affiché au démarrage dans le moniteur série. À enregistrer dans Orange Live Objects lors de la création du device.

| Canal | Type Cayenne LPP | Donnée | Unité | Résolution |
|-------|-----------------|--------|-------|------------|
| 1 | Temperature (0x67) | BME280 | °C | 0.1 |
| 2 | Humidity (0x68) | BME280 | % | 0.5 |
| 3 | Barometric Pressure (0x73) | BME280 | hPa | 0.1 |
| 4 | Illuminance (0x65) | BH1750 | lux | 1 |
| 5 | Analog Input (0x02) | Tension batterie | V | 0.01 |
| 6 | Analog Input (0x02) | Poids | kg | 0.01 |
| 7 | Analog Input (0x02) | Son / 100 | — | 0.01 |
| 8 | Digital Input (0x00) | Flag alerte | 0/1 | 1 |

Les canaux dont la valeur est indisponible (capteur absent ou désactivé) sont **omis** du payload pour réduire la taille de la trame.

### Horloge

- DS3231 synchronisé via NTP au démarrage (`pool.ntp.org`, fuseau `CET-1CEST`)
- Fallback sur l'heure NTP directement si DS3231 absent

---

## Procédure de flash

**Ordre obligatoire dans PlatformIO :**

1. `Upload Filesystem Image` — charge `data/` dans SPIFFS
2. `Upload` — charge le firmware

> Si seul le firmware est modifié (pas les fichiers `data/`), seule l'étape 2 est nécessaire.

---

## Dépendances

| Bibliothèque | Source | Rôle |
|---|---|---|
| Adafruit BME280 Library | adafruit/Adafruit BME280 Library | BME280 |
| BH1750 | claws/BH1750 | BH1750 |
| RTClib | adafruit/RTClib | DS3231 |
| RadioLib | jgromes/RadioLib@^6.6.0 | SX1262 / LoRaWAN |
| HX711 | bogde/HX711 *(commenté)* | Cellule de charge |
| WebServer | Espressif Arduino (natif) | Serveur HTTP |
| HTTPClient + WiFiClientSecure | Espressif Arduino (natif) | SMS OVH |
| W3.CSS 4.15 | Jan Egil & Borge Refsnes | Interface web |

---

## Interface web

Auto-refresh JavaScript toutes les **5 secondes** (capteurs) et **1 seconde** (heure).
CSS : W3.CSS 4.15 + thème sombre custom. Chemins `./` pour prévisualisation locale sans carte.

---

## Sécurité

- `secrets.h` est dans `.gitignore` — ne jamais committer ce fichier
- `secrets.h.example` est le seul template versionné
- HTTPS OVH : `setInsecure()` (pas de vérification CA) — suffisant pour cet usage embarqué
