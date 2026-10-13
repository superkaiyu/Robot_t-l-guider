# Robot téléguidé — 2 ESP32 + GPS + IHM, tout en WiFi

```
                          WiFi "ROBOT_ESP32" (créé par le robot, 192.168.4.1)
   ┌──────────────────────┐   UDP 4210, 25×/s : joystick + bouton     ┌───────────────────────────┐
   │ ESP32 MANETTE (XIAO)  │ ────────────────────────────────────────▶ │ ESP32 ROBOT (S3 N16R8)     │
   │ joystick (breadboard) │ ◀──────────────────────────────────────── │ driver moteurs + batterie  │
   └──────────────────────┘   réponse : qui pilote + position GPS    │                            │
                                                                     │  ▲ UART (fil TX du GPS)    │
   ┌──────────────────────┐   http://192.168.4.1  (page de l'IHM)    │  │                         │
   │ PC / téléphone        │ ◀──────────────────────────────────────── │ GPS Air530                 │
   │ IHM dans navigateur   │ ◀══ WebSocket port 81, 20×/s ══════════▶ │                            │
   └──────────────────────┘   télémétrie ⇄ commandes, ping           └───────────────────────────┘
```

- Le **robot** crée le réseau WiFi `ROBOT_ESP32` (mot de passe `motdepasse123`). Tout le monde s'y connecte : il n'y a besoin d'aucune box ni d'Internet.
- Le **GPS** est branché au robot par un fil série. Le robot décode la position et la diffuse en WiFi à l'IHM **et** à la Manette.
- La **Manette** envoie son joystick au robot 25×/s en UDP et reçoit en retour la position GPS (affichée dans son Moniteur Série).
- L'**IHM** est servie par le robot lui-même : ouvrir **http://192.168.4.1** suffit, elle se connecte toute seule en **WebSocket** (connexion permanente : le robot pousse la télémétrie 20×/s, les commandes partent dès qu'elles changent, la latence est affichée en haut à droite).

> **À propos du GPS :** un module GPS calcule sa position en *écoutant* les satellites (il n'émet rien vers eux). Cette partie est inévitable : **en intérieur, pas de position**. En revanche, tout le transport des données (GPS → ESP32 → IHM / Manette) passe uniquement par le fil série et le WiFi de l'ESP32. L'IHM affiche clairement si le module est **non détecté** (câblage), **en recherche** (câblé et OK, mais pas encore de ciel) ou a une **position acquise**.
> Pour tester toute la chaîne en intérieur, mettre `#define GPS_SIMULATION 1` dans `robot/robot.ino` : le robot envoie alors une fausse position (cercle de 10 m) marquée « Simulation ».

## Contenu

| Fichier | Rôle |
|---|---|
| `robot/robot.ino` | ESP32 **Robot** (ESP32-S3 N16R8) : point d'accès WiFi, GPS, moteurs, télémétrie, sert l'IHM |
| `robot/gps_detection.h`, `robot/gps_filtre.h` | Détection automatique du GPS, filtre de position |
| `robot/ws_serveur.h` | Mini serveur WebSocket (aucune bibliothèque à installer) |
| `robot/ihm_html.h` | IHM compressée embarquée dans le robot (**générée**, ne pas modifier à la main) |
| `robot/secrets_exemple.h` | Modèle pour les réseaux WiFi externes optionnels |
| `manette/manette.ino` | ESP32 **Manette** (Seeed XIAO ESP32S3) : joystick → robot, reçoit le GPS |
| `ihm/ihm.html` | Source de l'IHM (peut aussi être ouverte directement sur le PC) |
| `televerser_robot.bat`, `televerser_manette.bat`, `televerser_tout.bat`, `lister_esp32.bat` | Double-clic sous Windows : téléverse sur le bon ESP32 |
| `tools/televerser.py` | Le script appelé par les `.bat` (détecte quel port COM est quelle carte) |
| `tools/generer_ihm_h.py` | Régénère `robot/ihm_html.h` après une modification de l'IHM |

## Câblage

### Principe de la breadboard

- Les 5 trous d'une même rangée (a–e ou f–j) sont reliés entre eux. La rainure du milieu sépare les deux côtés.
- Les longues lignes des bords (rouge +, bleue −) sont des **rails** : un rail **3V3** (rouge) et un rail **masse** (bleu).
- La carte se pose à cheval sur la rainure : chaque broche tombe dans sa propre rangée. Pour la relier, plante un fil dans un des 4 autres trous de la même rangée, du même côté.

### ESP32 Robot : ESP32-S3 N16R8 (DevKitC-1)

Alimentation : **GND → rail bleu**, **3V3 → rail rouge**, câble USB-C sur le port marqué **UART** (PC, chargeur ou batterie externe).

| Élément | Branchement |
|---|---|
| **GPS** | **TX du module → GPIO14**, VCC → rail rouge, GND → rail bleu. Le RX du module reste libre. |
| LED de statut | GPIO21 → résistance 220–330 Ω → patte longue de la LED ; patte courte → rail bleu |
| Tension batterie | Pont diviseur : 30 kΩ entre le **+ batterie** et une rangée libre, 10 kΩ entre cette rangée et le rail bleu. Cette rangée → **GPIO1**. Le + batterie ne touche **jamais** directement un GPIO. |
| Courant | GPIO2. Sans capteur de courant, relier GPIO2 au rail bleu. |
| Driver moteur (signaux) | **GPIO15 → AIN1**, **GPIO16 → AIN2** (roue gauche), **GPIO17 → BIN1**, **GPIO18 → BIN2** (roue droite) |
| Driver : alimentation logique | TB6612FNG : VCC, STBY, PWMA, PWMB → rail rouge. DRV8833 : nSLEEP → rail rouge. |
| Driver : **masse commune** | GND du driver → rail bleu (sans ce fil, les commandes ne passent pas) |
| Moteurs | Gauche sur AO1/AO2, droite sur BO1/BO2 |
| Alimentation moteurs | + de l'alim/batterie → VM du driver, − → GND du driver. Le courant des moteurs passe en fils directs vers le driver, **jamais** par les rails de la breadboard. |

Une roue tourne à l'envers ? Dans `robot.ino`, passe `INVERSER_ROUE_G` ou `INVERSER_ROUE_D` à `true`. Les moteurs sont pilotés en PWM à 20 kHz, au-delà de l'audible, ce qui supprime le sifflement. Avec un vieux driver L298N, remets `FREQUENCE_PWM_MOTEURS` à 1000.

Sans batterie branchée sur le pont diviseur, l'IHM affiche la batterie « NON BRANCHÉE » (pas d'alarme).

**Arduino IDE (Robot)** : carte **ESP32S3 Dev Module** ; *USB CDC On Boot : Disabled* (le Moniteur Série passe par le port UART) ; *Flash Size : 16MB* ; *PSRAM : OPI PSRAM*. Le même `robot.ino` fonctionne encore sur une XIAO ESP32S3 : le brochage est choisi selon la carte sélectionnée.

**Détection automatique du GPS** : au démarrage, le Robot cherche tout seul sur quelle broche arrive le fil TX du GPS (**GPIO14**, puis les broches libres voisines GPIO13, 12, 11, 10, 4, 5, 6, 7) et à quelle vitesse il parle (9600, 115200, 38400, 57600, 19200, 4800 bauds). Il vérifie que ce sont de vraies trames GPS (NMEA avec somme de contrôle), puis retient la combinaison trouvée pour le démarrage suivant. Si le GPS est débranché puis rebranché, il le retrouve (nouvel essai toutes les 10 s). Le résultat s'affiche :

- dans le Moniteur Série du robot : `[GPS] ✅ Module GPS détecté sur GPIO14 à 9600 bauds.` ;
- dans l'IHM, onglet **GPS** (« Branché sur : GPIO14 · 9600 bauds »), avec un bouton **Relancer la détection du GPS** ;
- avec `lister_esp32.bat` : `COM5  ROBOT … GPS : module OK, recherche des satellites — branché sur GPIO14 à 9600 bauds`.

Commandes à taper dans le Moniteur Série du robot (115200 bauds) : `?` identité + état du GPS, `g` afficher les trames GPS brutes (marche/arrêt), `d` relancer la détection.

### ESP32 Manette : Seeed XIAO ESP32S3

La XIAO se pose à cheval sur la rainure (D0 à D10 sont imprimés sur la carte), alimentée par son propre câble USB-C.

| Élément | Broche XIAO | GPIO |
|---|---|---|
| Joystick +5V / VCC | **3V3** (même si le module indique 5V) | — |
| Joystick GND | GND | — |
| Joystick VRx | D2 | 3 |
| Joystick VRy | D10 | 9 |
| Joystick SW (bouton) | D6 | 43 |
| LED statut (via 220–330 Ω, cathode vers GND) | D5 | 6 |

**Arduino IDE (Manette)** : carte **XIAO_ESP32S3**, *USB CDC On Boot : Enabled*.

### Ordre conseillé

1. Câble le Robot **sans** les moteurs ni la batterie, téléverse-le et vérifie l'IHM sur http://192.168.4.1.
2. Câble la Manette et vérifie qu'elle se connecte au robot (LED fixe, pastille « ESP32 Manette » verte).
3. Branche le driver et les moteurs en dernier, avec une alimentation réglée à 6 V et une limite de courant basse.

## Téléverser les deux ESP32 (un seul port USB du PC)

Branche les deux cartes sur un **hub USB** (ou deux ports) : le Robot par son port USB-C **UART**, la Manette par son USB-C. Chaque carte a un numéro de série unique : Windows lui donne **son propre port COM** (par ex. COM5 et COM7), toujours le même ensuite. Rien à régler dans les cartes.

**Méthode automatique (recommandée)** — il faut seulement Python (https://www.python.org, cocher « Add to PATH ») et l'Arduino IDE 2 déjà utilisé :

1. Double-clic sur **`televerser_tout.bat`** (ou `televerser_robot.bat` / `televerser_manette.bat`).
2. Le script demande « ? » à chaque carte, qui répond `ID=ROBOT` ou `ID=MANETTE`, puis compile et téléverse le bon programme sur le bon port. Il utilise l'`arduino-cli` intégré à l'Arduino IDE : mêmes cartes, mêmes bibliothèques, et la bonne carte pour chaque programme (ESP32-S3 N16R8 pour le Robot, XIAO pour la Manette).
3. La toute première fois (cartes pas encore programmées), il te demande laquelle est laquelle, puis s'en souvient (`tools/cartes_esp32.json`).

`lister_esp32.bat` affiche simplement quel COM est le Robot et lequel est la Manette. En ligne de commande : `python tools/televerser.py moniteur robot` ouvre le Moniteur Série d'une carte.

**Méthode manuelle dans l'Arduino IDE** : ouvre `robot.ino` et `manette.ino` dans **deux fenêtres** de l'IDE. Dans chaque fenêtre, choisis la bonne carte (**ESP32S3 Dev Module** pour le Robot, **XIAO_ESP32S3** pour la Manette, réglages ci-dessus) et le port de la bonne carte (*Outils › Port*). Pour savoir quel COM est quelle carte, ouvre le Moniteur Série (115200 bauds), tape `?` puis Entrée : la carte répond `ID=ROBOT` ou `ID=MANETTE`. Tu peux aussi débrancher une carte et regarder quel port disparaît.

> ⚠️ Utilise le bouton **Téléverser (→)** et non **Déboguer** (icône insecte). Le débogueur lance OpenOCD sur l'interface JTAG, d'où l'erreur `LIBUSB_ERROR_ACCESS … esp_usb_jtag: could not find or open device` : ce n'est pas nécessaire pour programmer la carte.
>
> Si un téléversement échoue : ferme le Moniteur Série ouvert sur ce port. Si la carte ne répond plus, maintiens **BOOT**, appuie sur **RESET**, relâche **BOOT**, puis relance.

## Mise en route

1. **Arduino IDE** : installer le paquet de cartes *esp32* (Espressif) et la bibliothèque **TinyGPSPlus** (Mikal Hart). Réglages des cartes : voir « Câblage » ci-dessus.
2. *(Optionnel)* copier `robot/secrets_exemple.h` en `robot/secrets.h` et y mettre les mots de passe de tes réseaux (box, partage de connexion, WIFI-UP). Ce fichier n'est jamais envoyé sur GitHub. Sans lui, le robot fonctionne avec son seul point d'accès.
3. Téléverser les deux cartes (voir ci-dessus). *Ne pas toucher le joystick pendant la première seconde : sa position de repos est calibrée au démarrage.*
4. Connecter le PC ou le téléphone au WiFi **ROBOT_ESP32**, puis ouvrir **http://192.168.4.1**.

Ce qu'on doit voir :

- Moniteur Série du robot : `[WiFi] ESP32 Manette connectée !`, `[WebSocket] IHM n°0 connectée`, puis toutes les 5 s une ligne `[GPS] etat=...`.
- Moniteur Série de la Manette : `[Robot] Liaison établie : commandes et GPS échangés.` puis chaque seconde la position GPS reçue.
- LED de la Manette : **fixe** = tout communique ; clignote lent = WiFi OK mais robot muet ; clignote vite = WiFi introuvable.
- IHM : les pastilles en haut (Robot, ESP32 Manette, GPS, latence ≈ quelques ms et « WebSocket · 20 msg/s »).

## L'IHM

- **Joystick virtuel** à glisser (souris ou doigt), **clavier** (flèches ou ZQSD, *Espace* = arrêt d'urgence), ou **manette USB** branchée au PC.
- **Barres Roue G / Roue D** : la puissance réellement appliquée par le robot (pas seulement celle demandée).
- **Qui pilote** : IHM, ESP32 Manette ou arrêt d'urgence, en direct.
- Onglets **Vue d'ensemble**, **GPS** (position, satellites, trajet, liens OpenStreetMap / Google Maps), **Batterie**, **Roues**.
- Aucune bibliothèque externe : tout fonctionne sans Internet (seule la carte OpenStreetMap optionnelle en a besoin). Utilisable aussi sur téléphone.
- Si le robot a encore un ancien firmware (sans WebSocket), l'IHM repasse automatiquement en HTTP (« HTTP (secours) »).
- Robot non connecté : **mode démo** qui simule le robot.

## Qui pilote ?

Plusieurs sources peuvent être connectées en même temps. Le robot choisit, à chaque instant :

1. **Arrêt d'urgence** : bouton du joystick de la Manette, bouton rouge / *Espace* dans l'IHM, ou bouton B de la manette USB → coupe tout.
2. **ESP32 Manette**, dès que son stick est écarté du centre.
3. **IHM**.

Une source au repos ne bloque jamais les autres. Sans commande fraîche depuis 500 ms, les moteurs sont coupés.

## Protocoles

| Liaison | Format |
|---|---|
| Manette → Robot (UDP 4210) | `M,<rg>,<rd>,<jx>,<jy>,<bouton 0/1>,<rssi>` (rg/rd de -100 à 100) |
| Robot → Manette | `OK;pilote=...;gps=...;fix=0/1;lat=...;lon=...;sat=...` |
| IHM → Robot (WebSocket 81) | `c,<rg>,<rd>` commande · `stop` arrêt d'urgence · `p,<n>` ping |
| Robot → IHM | JSON de télémétrie 20×/s · `P,<n>` réponse au ping |
| HTTP (secours / outils) | `/` IHM · `/cmd?src=ihm&rg=..&rd=..[&stop=1]` · `/telemetrie` · `/gps` · `/gps-detecter` · `/wifiup` · `/aide` |

## Modifier l'IHM

Modifier `ihm/ihm.html`, puis `python tools/generer_ihm_h.py` et re-téléverser le robot. (`televerser_robot.bat` régénère le fichier automatiquement.)

## Dépannage GPS

| L'IHM affiche | Signification | Que faire |
|---|---|---|
| **Détection du module…** | Le robot essaie les broches / vitesses une par une | Attendre quelques secondes |
| **Module non détecté** | Rien reçu sur aucune des broches essayées, à aucune vitesse | Vérifier le fil TX du GPS → GPIO14 du Robot, VCC → 3V3 (ou 5V selon le module), GND commun, LED du module allumée |
| **Données illisibles** | Des caractères arrivent mais ne forment jamais de trames NMEA valides | Masse (GND) commune, vitesse du module inhabituelle |
| **Recherche · N satellites entendus** | Câblage OK, le module communique, pas encore de position | Aller dehors / près d'une fenêtre, attendre 1 à 5 min au premier démarrage |
| **Position acquise** | Tout fonctionne | — |

**Le tracé GPS bougeait tout seul, robot immobile ?** C'est normal pour un GPS : chaque mesure varie de 2 à 5 m dehors, et de 10 à 20 m en intérieur. Le robot filtre maintenant la position (`robot/gps_filtre.h`) :
- **moteurs arrêtés** : la position est moyennée pendant 30 s puis **figée**. Le trajet ne bouge plus, même si le GPS dérive de plusieurs dizaines de mètres (fréquent en intérieur) ou annonce une fausse vitesse de 1 à 3 km/h ;
- **mesures imprécises (HDOP > 8) et sauts aberrants** : ignorés ;
- **robot qui roule (moteurs), ou GPS vraiment porté à la main** (plus de 3,5 km/h pendant 5 s et au moins 4 m parcourus) : le trajet suit.

Dans l'onglet GPS, les points pâles montrent les mesures brutes (le bruit) ; la ligne est le trajet filtré.
