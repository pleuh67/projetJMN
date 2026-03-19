# Station de mesure JMN — Guide utilisateur

## Présentation

La carte **XIAO ESP32S3** héberge un petit serveur web accessible depuis n'importe quel navigateur sur le réseau Wi-Fi.
Elle permet de :

- **Allumer / éteindre** la LED intégrée
- **Consulter en temps réel** la température, l'humidité, la pression atmosphérique et la luminosité
- **Recevoir une alerte SMS** lorsqu'un seuil est dépassé
- **Transmettre les mesures par LoRa** vers le réseau Orange, toutes les 5 minutes

---

## Matériel connecté

| Capteur | Ce qu'il mesure | Protocole |
|---------|-----------------|-----------|
| BME280 | Température (°C), Humidité (%), Pression (hPa) | I2C |
| BH1750 | Luminosité (lux) | I2C |
| DS3231 | Horloge temps réel (date et heure) | I2C |
| Wio-SX1262 | Module LoRa (émission vers réseau Orange) | SPI |

> **Capteurs en préparation** (seront activés sur la prochaine carte) :
> - Microphone INMP441 — niveau sonore en dB
> - Cellule de charge HX711 — poids en kg

---

## Câblage I2C (XIAO ESP32S3)

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
- BH1750 : `0x23` (ADDR → GND)
- DS3231 : `0x68` (fixe)

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
│  └──────────┴────────────┘  │
│  Mis à jour : 14:32:01      │
└─────────────────────────────┘
```

---

## Transmission LoRa (Orange Live Objects)

La carte envoie automatiquement les mesures **toutes les 15 minutes** via le réseau LoRa Orange.
Les données sont visibles dans l'interface Orange Live Objects.

### Paramètres de déclaration du device (Orange Live Objects)

| Paramètre | Valeur |
|-----------|--------|
| DevEUI | `744DBDFFFE995780` |
| Profil | `Generic_classA_RX2SF12` |
| Classe | Class A |
| Activation | OTAA |
| Plan de fréquences | EU868 |
| AppKey | *voir `secrets.h`* |
| JoinEUI | *voir `secrets.h`* |

Chaque trame contient :
- Température, humidité, pression atmosphérique
- Luminosité
- Tension batterie *(si câblée)*
- Poids *(quand le capteur sera connecté)*
- Niveau sonore *(quand le microphone sera connecté)*
- Un flag d'alerte (0 = normal, 1 = alerte)

En cas d'alerte (niveau sonore dépassé), une trame avec le flag alerte=1 est envoyée **immédiatement**, en plus de l'envoi périodique.

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
  "sound_db": null
}
```

> `null` signifie que le capteur correspondant n'est pas connecté ou pas activé.

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
{ "datetime": "2026-03-18 14:32:01" }
```

---

## Dépannage

| Symptôme | Cause probable | Solution |
|----------|---------------|----------|
| Valeurs `--` sur les capteurs | Capteur non détecté | Vérifier le câblage SDA/SCL et l'alimentation |
| Impossible d'accéder à l'IP | Wi-Fi non connecté | Vérifier SSID/mot de passe dans `secrets.h`, surveiller le moniteur série |
| LED ne réagit pas | Logique inversée | La LED intégrée est active-bas (LOW = allumée) |
| BME280 non détecté | Mauvaise adresse I2C | Relier SDO à 3.3V pour passer en adresse `0x77` |
| SMS non reçu | Anti-spam actif | Attendre 5 min entre deux envois ; vérifier les logs `[SMS]` dans le moniteur série |
| LoRa : join échoué | Credentials LoRaWAN incorrects | Vérifier `LORAWAN_JOIN_EUI`, `LORAWAN_DEV_EUI`, `LORAWAN_APP_KEY` dans `secrets.h` |
| LoRa : trame non reçue | Couverture réseau | Vérifier la couverture Orange LoRa sur le site, ou rapprocher d'une fenêtre |

---

## Architecture des fichiers web (SPIFFS)

Les fichiers de l'interface sont stockés dans la **mémoire flash SPIFFS** de la carte, dans le dossier `data/` du projet :

| Fichier | Rôle |
|---------|------|
| `data/index.html` | Page principale de l'interface |
| `data/w3.min.css` | Framework CSS W3.CSS 4.15 (mise en forme) |
| `data/style.css` | Thème sombre personnalisé |
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
| Adafruit BME280 Library | Adafruit | Lecture température / humidité / pression |
| BH1750 | claws | Lecture luminosité |
| RTClib | Adafruit | Horloge temps réel DS3231 |
| RadioLib | jgromes | Module LoRa SX1262 / LoRaWAN |
| WebServer | Espressif (Arduino) | Serveur HTTP intégré |
| SPIFFS | Espressif (Arduino) | Système de fichiers flash |
| HTTPClient + WiFiClientSecure | Espressif (Arduino) | Envoi SMS via API OVH (HTTPS) |
| W3.CSS 4.15 | Jan Egil & Borge Refsnes | Framework CSS de mise en forme |

---

*Projet ProjetJMN — XIAO ESP32S3 — mis à jour le 19 mars 2026*
