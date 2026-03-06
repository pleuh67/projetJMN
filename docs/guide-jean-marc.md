# Station de mesure JMN — Guide utilisateur

## Présentation

La carte **XIAO ESP32S3** héberge un petit serveur web accessible depuis n'importe quel navigateur sur le réseau Wi-Fi.
Elle permet de :

- **Allumer / éteindre** la LED intégrée
- **Consulter en temps réel** la température, l'humidité, la pression atmosphérique et la luminosité ambiante

---

## Matériel connecté

| Capteur  | Ce qu'il mesure                          | Protocole |
|----------|------------------------------------------|-----------|
| BME280   | Température (°C), Humidité (%), Pression (hPa) | I2C |
| BH1750   | Luminosité (lux)                         | I2C       |

### Câblage I2C (XIAO ESP32S3)

```
XIAO ESP32S3        BME280 / BH1750
────────────        ───────────────
3.3V           →    VCC
GND            →    GND
GPIO 5 (SDA)   →    SDA
GPIO 6 (SCL)   →    SCL
```

> Les deux capteurs partagent le même bus I2C (même 4 fils), il suffit de les brancher en parallèle.

**Adresses I2C :**
- BME280 : `0x76` (SDO relié à GND) ou `0x77` (SDO relié à 3.3V)
- BH1750 : `0x23` (ADDR relié à GND) ou `0x5C` (ADDR relié à 3.3V)

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

## Endpoint JSON (pour intégration)

L'URL `/sensors` retourne les données brutes au format JSON :

```
GET http://<adresse-ip>/sensors
```

Exemple de réponse :
```json
{
  "temperature": 22.54,
  "humidity": 58.20,
  "pressure": 1013.25,
  "lux": 320.0
}
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

---

## Bibliothèques utilisées

| Bibliothèque | Auteur | Rôle |
|---|---|---|
| `Adafruit BME280 Library` | Adafruit | Lecture température / humidité / pression |
| `BH1750` | claws | Lecture luminosité |
| `WebServer` | Espressif (Arduino) | Serveur HTTP intégré |

---

*Projet ProjetJMN — XIAO ESP32S3*
