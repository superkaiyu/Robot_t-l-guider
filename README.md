# Robot téléguidé — 2 ESP32 + GPS + IHM, tout en WiFi

```
                         WiFi "ROBOT_ESP32" (créé par le robot, 192.168.4.1)
   ┌─────────────────────┐      /cmd?src=manette&rg..&jx..&jb..      ┌──────────────────────────┐
   │ ESP32 MANETTE        │ ────────────────────────────────────────▶ │ ESP32 ROBOT               │
   │ joystick (breadboard)│ ◀──────────────────────────────────────── │ moteurs + batterie        │
   └─────────────────────┘      réponse : pilote + position GPS      │                           │
                                                                     │  ▲ UART (fil TX du GPS)   │
   ┌─────────────────────┐      GET /  (page IHM servie par le robot)│  │                        │
   │ PC / téléphone       │ ◀──────────────────────────────────────── │ GPS Air530                │
   │ IHM dans navigateur  │ ◀── /telemetrie (GPS, joystick, batterie) │                           │
   └─────────────────────┘ ──▶ /cmd?src=ihm (croix, manette USB)     └──────────────────────────┘
```

- Le **robot** crée le réseau WiFi `ROBOT_ESP32` (mot de passe `motdepasse123`). Tout le monde s'y connecte : il n'y a besoin d'aucune box ni d'Internet.
- Le **GPS** est branché au robot par un fil série. Le robot décode la position et la diffuse en WiFi à l'IHM **et** à la Manette.
- La **Manette** envoie le joystick au robot 10×/s et reçoit en retour la position GPS (affichée dans son Moniteur Série).
- L'**IHM** est servie par le robot lui-même : ouvrir **http://192.168.4.1** suffit, elle se connecte toute seule.

> **À propos du GPS :** un module GPS calcule sa position en *écoutant* les satellites (il n'émet rien vers eux). Cette partie est inévitable : **en intérieur, pas de position**. En revanche, tout le transport des données (GPS → ESP32 → IHM / Manette) passe uniquement par le fil série et le WiFi de l'ESP32. L'IHM affiche clairement si le module est **non détecté** (câblage), **en recherche** (câblé et OK, mais pas encore de ciel) ou a une **position acquise**.
> Pour tester toute la chaîne en intérieur, mettre `#define GPS_SIMULATION 1` dans `robot/robot.ino` : le robot envoie alors une fausse position (cercle de 10 m) marquée « Simulation ».

## Contenu

| Dossier | Rôle |
|---|---|
| `robot/robot.ino` | ESP32 **Robot** : point d'accès WiFi, GPS, moteurs, télémétrie, sert l'IHM |
| `robot/ihm_html.h` | IHM embarquée dans le robot (**générée**, ne pas modifier à la main) |
| `robot/secrets_exemple.h` | Modèle pour les réseaux WiFi externes optionnels |
| `manette/manette.ino` | ESP32 **Manette** : joystick → robot, reçoit le GPS |
| `ihm/ihm.html` | Source de l'IHM (peut aussi être ouverte directement sur le PC) |
| `tools/generer_ihm_h.py` | Régénère `robot/ihm_html.h` après une modification de l'IHM |

## Câblage (Seeed XIAO ESP32S3)

**ESP32 Robot**

| Élément | Broche XIAO | GPIO |
|---|---|---|
| **GPS TX** (le fil qui sort du GPS) | **D7** | 44 |
| GPS VCC / GND | 3V3 / GND | — |
| Roue gauche PWM avant / arrière | D0 / D1 | 1 / 2 |
| Roue droite PWM avant / arrière | D3 / D4 | 4 / 5 |
| LED statut | D5 | 6 |
| Mesure batterie / courant | D8 / D9 | 7 / 8 |

Le fil RX du GPS n'est pas utilisé (lecture seule).

**ESP32 Manette**

| Élément | Broche XIAO | GPIO |
|---|---|---|
| Joystick VRx | D2 | 3 |
| Joystick VRy | D10 | 9 |
| Joystick SW (bouton) | D6 | 43 |
| Joystick VCC / GND | 3V3 / GND | — |
| LED statut (via 220–330 Ω) | D5 | 6 |

## Mise en route

1. **Arduino IDE** : installer le paquet de cartes *esp32* (Espressif), choisir la carte **XIAO_ESP32S3**, et installer la bibliothèque **TinyGPSPlus** (Mikal Hart).
2. *(Optionnel)* copier `robot/secrets_exemple.h` en `robot/secrets.h` et y mettre les mots de passe de tes réseaux (box, partage de connexion, WIFI-UP). Ce fichier n'est jamais envoyé sur GitHub. Sans lui, le robot fonctionne avec son seul point d'accès.
3. Téléverser `robot/robot.ino` sur l'ESP32 Robot, puis `manette/manette.ino` sur l'ESP32 Manette.
   *Ne pas toucher le joystick pendant la première seconde : sa position de repos est calibrée au démarrage.*
4. Connecter le PC ou le téléphone au WiFi **ROBOT_ESP32**, puis ouvrir **http://192.168.4.1**.

Ce qu'on doit voir :

- Moniteur Série du robot : `[WiFi] ESP32 Manette connectée !` puis toutes les 5 s une ligne `[GPS] etat=...`.
- Moniteur Série de la Manette : `[Robot] Liaison établie : commandes et GPS échangés.` puis chaque seconde la position GPS reçue.
- LED de la Manette : **fixe** = tout communique ; clignote lent = WiFi OK mais robot muet ; clignote vite = WiFi introuvable.
- IHM : trois voyants en haut (Robot, ESP32 Manette, GPS), le stick de la Manette qui bouge en direct, et l'onglet **GPS** (position, satellites, trajet, liens OpenStreetMap / Google Maps).

## Qui pilote ?

Plusieurs sources peuvent être connectées en même temps. Le robot choisit, à chaque instant :

1. **Arrêt d'urgence** : bouton du joystick de la Manette, bouton rouge de l'IHM ou bouton B de la manette USB → coupe tout.
2. **Joystick local** du robot (seulement si `JOYSTICK_LOCAL 1`).
3. **ESP32 Manette**, dès que son stick est écarté du centre.
4. **IHM** (croix de secours, manette USB du PC).

Une source au repos ne bloque jamais les autres, et sans commande fraîche depuis 500 ms les moteurs sont coupés. L'IHM affiche « Qui pilote le robot ».

## Routes HTTP du robot

| Route | Usage |
|---|---|
| `/` | IHM complète |
| `/cmd?src=manette\|ihm&rg=-100..100&rd=-100..100` | Commande des roues (+ `stop=1`, et `jx`, `jy`, `jb`, `rssi` pour la Manette) |
| `/telemetrie` | JSON complet (batterie, joystick, GPS, état du réseau) |
| `/gps` | JSON avec uniquement le GPS |
| `/wifiup` | Connexion au portail captif WIFI-UP (à compléter, voir `robot.ino`) |
| `/aide` | Liste des routes |

## Modifier l'IHM

Modifier `ihm/ihm.html`, puis :

```sh
python3 tools/generer_ihm_h.py
```

et re-téléverser `robot/robot.ino`. L'IHM n'utilise aucune bibliothèque externe, donc elle fonctionne sans Internet (le PC connecté à `ROBOT_ESP32` n'a pas Internet). Seule la carte OpenStreetMap optionnelle en a besoin.

## Dépannage GPS

| L'IHM affiche | Signification | Que faire |
|---|---|---|
| **Module non détecté** | Le robot ne reçoit aucun caractère du GPS | Vérifier GPS TX → D7, VCC → 3V3, GND commun |
| **Données illisibles** | Des caractères arrivent mais ne forment pas de trames NMEA valides | Vitesse série (9600 bauds pour l'Air530), masse commune |
| **Recherche... (N satellites entendus)** | Câblage OK, le module communique, pas encore de position | Aller dehors / près d'une fenêtre, attendre 1 à 5 min au premier démarrage |
| **Position acquise** | Tout fonctionne | — |
