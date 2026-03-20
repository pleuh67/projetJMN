# Station de mesure JMN — Guide utilisateur

## Présentation

La carte **XIAO ESP32S3** héberge un petit serveur web accessible depuis n'importe quel navigateur sur le réseau Wi-Fi.
Elle permet de :

- **Allumer / éteindre** la LED intégrée
- **Consulter en temps réel** le niveau sonore (si microphone connecté)
- **Recevoir une alerte SMS** lorsqu'un seuil est dépassé
- **Transmettre les mesures par LoRa** vers le réseau Orange, toutes les 5 minutes

---

## Matériel connecté

| Capteur | Ce qu'il mesure | Protocole |
|---------|-----------------|-----------|
| Wio-SX1262 | Module LoRa (émission vers réseau Orange) | SPI |

> **Capteurs en préparation** (seront activés sur la prochaine carte) :
> - Microphone INMP441 — niveau sonore en dB
> - Cellule de charge HX711 — poids en kg

---

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
│  ┌──────────────────────┐   │
│  │  Son : 68.3 dB       │   │
│  └──────────────────────┘   │
│  Mis à jour : 14:32:01      │
└─────────────────────────────┘
```

---

## Transmission LoRa (Orange Live Objects)

La carte envoie automatiquement les données **toutes les 5 minutes** via le réseau LoRa Orange.
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

### Processus de connexion au réseau (join OTAA)

La connexion au réseau LoRa fonctionne en **OTAA** (Over The Air Activation) : la carte négocie une session chiffrée avec le réseau Orange avant de pouvoir envoyer des données.

#### Au premier démarrage

```
[LoRa] Pas de session en NVS (premier demarrage)
[2026-03-19 15:25:46] LoRaWAN OTAA join Orange Live Objects... joint (nouveau) !
[LoRa] Session sauvegardee en NVS
```

1. La carte envoie un **JoinRequest** au réseau Orange (trame radio).
2. Orange vérifie les credentials (DevEUI, AppKey, JoinEUI) et répond avec un **JoinAccept**.
3. La carte reçoit le JoinAccept → session établie → **session sauvegardée en mémoire flash**.
4. Un premier payload de confirmation est envoyé immédiatement.

#### Aux démarrages suivants

```
[LoRa] Session NVS chargee
[2026-03-19 15:30:01] LoRaWAN OTAA join Orange Live Objects... session restauree (pas de JoinRequest) !
```

La session précédente est restaurée depuis la mémoire flash : **aucun JoinRequest radio n'est émis**. La carte est immédiatement opérationnelle.

#### En cas d'échec du join (hors portée au premier démarrage)

```
[LoRa] Pas de session en NVS (premier demarrage)
[2026-03-19 15:25:46] LoRaWAN OTAA join Orange Live Objects... join echoue : -1116
```

La carte continue de fonctionner (serveur web, capteurs) et **retente le join toutes les 5 minutes** automatiquement. Dès qu'elle est en portée d'une antenne Orange, la connexion s'établit.

#### En cas de déconnexion longue (session expirée)

Si la carte est hors portée pendant une longue période, Orange peut invalider la session. La carte le détecte automatiquement après **3 envois consécutifs en échec** :

```
[LoRa] Erreur envoi : -1 (1/3 echecs consecutifs)
[LoRa] Erreur envoi : -1 (2/3 echecs consecutifs)
[LoRa] Erreur envoi : -1 (3/3 echecs consecutifs)
[LoRa] Seuil echecs atteint — session invalidee, re-join au prochain cycle
```

La session est effacée et un nouveau JoinRequest est envoyé au cycle suivant.

#### LED et état de connexion

| État | LED |
|------|-----|
| Veille normale | Flash court toutes les 2 s |
| Join en cours / envoi LoRa | LED allumée en continu |
| Hors portée (join échoue) | Flash toutes les 2 s (fonctionnement normal sinon) |

### Contenu des trames

Chaque trame contient :
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
  "sound_db": 68.3
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
| SMS non reçu | Anti-spam actif | Attendre 5 min entre deux envois ; vérifier les logs `[SMS]` dans le moniteur série |
| LoRa : join échoué `-1116` | Device non déclaré ou credentials incorrects | Vérifier `LORAWAN_JOIN_EUI` et `LORAWAN_APPKEY_PREFIX` dans `secrets.h` ; recréer le device sur Orange Live Objects |
| LoRa : join échoué en boucle | Hors portée d'une gateway Orange | Rapprocher d'une fenêtre ou vérifier la couverture sur le site Orange |
| LoRa : session expirée après longue absence | Orange invalide les sessions inactives | La carte détecte automatiquement 3 échecs consécutifs et refait un join |
| LoRa : trame non reçue sur Orange | Compteur de trames désynchronisé | Recréer le device sur Orange Live Objects pour réinitialiser les compteurs |

---

## Architecture du projet

### Fichiers source (firmware)

| Fichier | Rôle |
|---------|------|
| `src/main.cpp` | Démarrage, WiFi, NTP, orchestration générale, boucle principale |
| `src/mesures.cpp` | Capteur INMP441 (son), niveau sonore en dB |
| `src/web.cpp` | Serveur HTTP, routes, handlers JSON, SPIFFS |
| `src/lora_lorawan.cpp` | Module LoRa SX1262, join OTAA, envoi Cayenne LPP, persistance session |
| `src/sms_ovh.cpp` | Alertes SMS via API OVH |
| `include/secrets.h` | Credentials WiFi, OVH, LoRaWAN (**gitignorée — ne pas partager**) |

### Fichiers web (interface SPIFFS)

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
2. **Upload** → charge le firmware

---

## Bibliothèques utilisées

| Bibliothèque | Auteur | Rôle |
|---|---|---|
| RadioLib | jgromes | Module LoRa SX1262 / LoRaWAN |
| WebServer | Espressif (Arduino) | Serveur HTTP intégré |
| SPIFFS | Espressif (Arduino) | Système de fichiers flash |
| HTTPClient + WiFiClientSecure | Espressif (Arduino) | Envoi SMS via API OVH (HTTPS) |
| W3.CSS 4.15 | Jan Egil & Borge Refsnes | Framework CSS de mise en forme |

---

*Projet ProjetJMN — XIAO ESP32S3 — mis à jour le 19 mars 2026 (v3)*
