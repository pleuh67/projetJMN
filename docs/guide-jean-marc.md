# Station de mesure JMN — Guide utilisateur

## Présentation

La carte **XIAO ESP32S3** héberge un petit serveur web accessible depuis n'importe quel navigateur sur le réseau Wi-Fi.
Elle permet de :

- **Allumer / éteindre** la LED intégrée
- **Consulter en temps réel** la température, l'humidité, la pression atmosphérique, la luminosité et le niveau sonore
- **Recevoir une alerte SMS** lorsqu'un seuil est dépassé

---

## Matériel connecté

| Capteur  | Ce qu'il mesure                               | Protocole |
|----------|-----------------------------------------------|-----------|
| BME280   | Température (°C), Humidité (%), Pression (hPa) | I2C       |
| BH1750   | Luminosité (lux)                              | I2C       |
| INMP441  | Niveau sonore (dB SPL approx.)                | I2S       |
| DS3231   | Horloge temps réel (date et heure)            | I2C       |

### Câblage I2C (XIAO ESP32S3)

```
XIAO ESP32S3        BME280 / BH1750 / DS3231
────────────        ──────────────────────────
3.3V           →    VCC
GND            →    GND
GPIO 5 (SDA)   →    SDA
GPIO 6 (SCL)   →    SCL
```

> Les capteurs I2C partagent le même bus (même 4 fils), ils se branchent en parallèle.

**Adresses I2C :**
- BME280 : `0x76` (SDO → GND) ou `0x77` (SDO → 3.3V)
- BH1750 : `0x23` (ADDR → GND) ou `0x5C` (ADDR → 3.3V)
- DS3231 : `0x68` (fixe)

### Câblage INMP441 (microphone I2S)

```
XIAO ESP32S3        INMP441
────────────        ───────
3.3V           →    VDD
GND            →    GND + L/R
GPIO 7 (D8)    →    SCK / BCLK
GPIO 8 (D9)    →    WS / LRCLK
GPIO 9 (D10)   →    SD / DATA
```

---

## Accéder à l'interface web

1. Mettre la carte sous tension (USB ou batterie).
2. Attendre ~5 secondes que la LED clignote puis reste fixe.
3. Ouvrir un navigateur sur un appareil connecté au même Wi-Fi.
4. Taper l'adresse IP affichée dans le moniteur série, par exemple :
   `http://192.168.1.42`

L'interface se met à jour **automatiquement toutes les 5 secondes** sans recharger la page.

---

## Description de l'interface

```
┌─────────────────────────────┐
│        Station JMN          │
│                             │
│          ● (LED)            │
│   LED : Allumée             │
│  [Allumer]  [Eteindre]      │
│                             │
│          CAPTEURS           │
│  ┌──────────┬────────────┐  │
│  │  22.5 °C │   58.2 %   │  │
│  │  Temp.   │  Humidité  │  │
│  ├──────────┼────────────┤  │
│  │ 1013 hPa │  320 lux   │  │
│  │ Pression │ Luminosité │  │
│  ├──────────┴────────────┤  │
│  │      68.3 dB          │  │
│  │  Niveau sonore        │  │
│  └───────────────────────┘  │
│  Mis à jour : 14:32:01      │
└─────────────────────────────┘
```

---

## Alertes SMS

La carte peut envoyer un SMS automatique lorsqu'une condition est détectée (par exemple : niveau sonore trop élevé).
- Un seul SMS est envoyé par déclenchement (anti-spam : minimum 5 minutes entre deux SMS).
- L'envoi est visible dans le moniteur série avec le préfixe `[SMS]`.

---

## Endpoints JSON (pour intégration)

### Données capteurs

```
GET http://<adresse-ip>/sensors
```

Exemple de réponse :
```json
{
  "temperature": 22.54,
  "humidity": 58.20,
  "pressure": 1013.25,
  "lux": 320.0,
  "sound_db": 68.3
}
```

### État LED

```
GET http://<adresse-ip>/state
```

Exemple de réponse :
```json
{ "led": true }
```

### Heure

```
GET http://<adresse-ip>/time
```

Exemple de réponse :
```json
{ "datetime": "2026-03-12 14:32:01" }
```

Si un capteur n'est pas détecté, sa valeur sera `null`.

---

## Dépannage

| Symptôme | Cause probable | Solution |
|----------|---------------|----------|
| Valeurs `--` sur tous les capteurs | Capteur non détecté | Vérifier le câblage SDA/SCL et l'alimentation |
| Impossible d'accéder à l'IP | Wi-Fi non connecté | Vérifier SSID/mot de passe dans `secrets.h`, surveiller le moniteur série |
| LED ne réagit pas | Logique inversée | La LED intégrée est active-bas (LOW = allumée) |
| BME280 non détecté | Mauvaise adresse I2C | Essayer de relier SDO à 3.3V pour passer en adresse `0x77` |
| SMS non reçu | Anti-spam actif | Attendre 5 min entre deux envois ; vérifier les logs `[SMS]` dans le moniteur série |
| `sound_db: null` | INMP441 non initialisé | Vérifier le câblage I2S (GPIO 7/8/9) |

---

## Architecture des fichiers web (SPIFFS)

Les fichiers de l'interface sont stockés dans la **mémoire flash SPIFFS** de la carte, dans le dossier `data/` du projet :

| Fichier | Rôle |
|---------|------|
| `data/index.html` | Page principale de l'interface |
| `data/w3.min.css` | Framework CSS W3.CSS 4.15 (mise en forme) |
| `data/style.css` | Thème sombre personnalisé (overrides) |
| `data/app.js` | Logique JavaScript (LED + capteurs) |

> Les chemins CSS utilisent `./` (relatif) ce qui permet aussi d'ouvrir `index.html` directement depuis le disque pour prévisualiser la mise en forme, sans avoir besoin de la carte.

### Procédure de mise à jour du firmware

Si les fichiers web sont modifiés, **deux flashages** sont nécessaires dans PlatformIO, dans cet ordre :

1. **Upload Filesystem Image** → charge `data/` dans SPIFFS
2. **Upload** → charge le firmware (`main.cpp`)

---

## Bibliothèques utilisées

| Bibliothèque | Auteur | Rôle |
|---|---|---|
| `Adafruit BME280 Library` | Adafruit | Lecture température / humidité / pression |
| `BH1750` | claws | Lecture luminosité |
| `RTClib` | Adafruit | Horloge temps réel DS3231 |
| `WebServer` | Espressif (Arduino) | Serveur HTTP intégré |
| `SPIFFS` | Espressif (Arduino) | Système de fichiers flash |
| `HTTPClient` + `WiFiClientSecure` | Espressif (Arduino) | Envoi SMS via API OVH (HTTPS) |
| `W3.CSS 4.15` | Jan Egil & Borge Refsnes | Framework CSS de mise en forme |

---

*Projet ProjetJMN — XIAO ESP32S3*
