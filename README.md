# Robot téléguidé — 2 ESP32 + GPS + IHM, tout en WiFi

```
                          WiFi "ROBOT_ESP32" (créé par le robot, 192.168.4.1)
   ┌──────────────────────┐   UDP 4210, 25×/s : joystick + bouton     ┌───────────────────────────┐
   │ ESP32 MANETTE         │ ────────────────────────────────────────▶ │ ESP32 ROBOT                │
   │ joystick (breadboard) │ ◀──────────────────────────────────────── │ moteurs + batterie         │
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
| `robot/robot.ino` | ESP32 **Robot** : point d'accès WiFi, GPS, moteurs, télémétrie, sert l'IHM |
| `robot/ws_serveur.h` | Mini serveur WebSocket (aucune bibliothèque à installer) |
| `robot/ihm_html.h` | IHM compressée embarquée dans le robot (**générée**, ne pas modifier à la main) |
| `robot/secrets_exemple.h` | Modèle pour les réseaux WiFi externes optionnels |
| `manette/manette.ino` | ESP32 **Manette** : joystick → robot, reçoit le GPS |
| `ihm/ihm.html` | Source de l'IHM (peut aussi être ouverte directement sur le PC) |
| `televerser_robot.bat`, `televerser_manette.bat`, `televerser_tout.bat`, `lister_esp32.bat` | Double-clic sous Windows : téléverse sur le bon ESP32 |
| `tools/televerser.py` | Le script appelé par les `.bat` (détecte quel port COM est quelle carte) |
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

**Détection automatique du GPS** : au démarrage, l'ESP32 Robot (celui **sans** joystick) cherche tout seul sur quelle broche arrive le fil TX du GPS (**D7**, puis D6, D2, D10) et à quelle vitesse il parle (9600, 115200, 38400, 57600, 19200, 4800 bauds). Il vérifie que ce sont de vraies trames GPS (NMEA avec somme de contrôle), puis retient la combinaison trouvée pour le démarrage suivant. Si le GPS est débranché puis rebranché, il le retrouve (nouvel essai toutes les 10 s). Le résultat s'affiche :

- dans le Moniteur Série du robot : `[GPS] ✅ Module GPS détecté sur D7 (GPIO44) à 9600 bauds.` ;
- dans l'IHM, onglet **GPS** (« Branché sur : D7 (GPIO44) · 9600 bauds »), avec un bouton **Relancer la détection du GPS** ;
- avec `lister_esp32.bat` : `COM5  ROBOT … GPS : module OK, recherche des satellites — branché sur D7 à 9600 bauds`.

Commandes à taper dans le Moniteur Série du robot (115200 bauds) : `?` identité + état du GPS, `g` afficher les trames GPS brutes (marche/arrêt), `d` relancer la détection.

**ESP32 Manette**

| Élément | Broche XIAO | GPIO |
|---|---|---|
| Joystick VRx | D2 | 3 |
| Joystick VRy | D10 | 9 |
| Joystick SW (bouton) | D6 | 43 |
| Joystick VCC / GND | 3V3 / GND | — |
| LED statut (via 220–330 Ω) | D5 | 6 |

## Téléverser les deux ESP32 (un seul port USB du PC)

Branche les deux XIAO sur un **hub USB** (ou deux ports). Chaque carte a un numéro de série unique : Windows lui donne **son propre port COM** (par ex. COM5 et COM7), toujours le même ensuite. Rien à régler dans les cartes.

**Méthode automatique (recommandée)** — il faut seulement Python (https://www.python.org, cocher « Add to PATH ») et l'Arduino IDE 2 déjà utilisé :

1. Double-clic sur **`televerser_tout.bat`** (ou `televerser_robot.bat` / `televerser_manette.bat`).
2. Le script demande « ? » à chaque carte, qui répond `ID=ROBOT` ou `ID=MANETTE`, puis compile et téléverse le bon programme sur le bon port. Il utilise l'`arduino-cli` intégré à l'Arduino IDE : mêmes cartes, mêmes bibliothèques.
3. La toute première fois (cartes pas encore programmées), il te demande laquelle est laquelle, puis s'en souvient (`tools/cartes_esp32.json`).

`lister_esp32.bat` affiche simplement quel COM est le Robot et lequel est la Manette. En ligne de commande : `python tools/televerser.py moniteur robot` ouvre le Moniteur Série d'une carte.

**Méthode manuelle dans l'Arduino IDE** : ouvre `robot.ino` et `manette.ino` dans **deux fenêtres** de l'IDE. Dans chaque fenêtre, choisis la carte **XIAO_ESP32S3** et le port de la bonne carte (*Outils › Port*). Pour savoir quel COM est quelle carte, ouvre le Moniteur Série (115200 bauds), tape `?` puis Entrée : la carte répond `ID=ROBOT` ou `ID=MANETTE`. Tu peux aussi débrancher une carte et regarder quel port disparaît.

> ⚠️ Utilise le bouton **Téléverser (→)** et non **Déboguer** (icône insecte). Le débogueur lance OpenOCD sur l'interface JTAG, d'où l'erreur `LIBUSB_ERROR_ACCESS … esp_usb_jtag: could not find or open device` : ce n'est pas nécessaire pour programmer la carte.
>
> Si un téléversement échoue : ferme le Moniteur Série ouvert sur ce port. Si la carte ne répond plus, maintiens **BOOT**, appuie sur **RESET**, relâche **BOOT**, puis relance.

## Mise en route

1. **Arduino IDE** : installer le paquet de cartes *esp32* (Espressif) et la bibliothèque **TinyGPSPlus** (Mikal Hart). Carte : **XIAO_ESP32S3**, avec *USB CDC On Boot : Enabled*.
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
2. **Joystick local** du robot (seulement si `JOYSTICK_LOCAL 1`).
3. **ESP32 Manette**, dès que son stick est écarté du centre.
4. **IHM**.

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
| **Module non détecté** | Rien reçu sur D7, D6, D2, D10, à aucune vitesse | Vérifier le fil TX du GPS (sur l'ESP32 **sans** joystick), VCC → 3V3 (ou 5V selon le module), GND commun, LED du module allumée |
| **Données illisibles** | Des caractères arrivent mais ne forment jamais de trames NMEA valides | Masse (GND) commune, vitesse du module inhabituelle |
| **Recherche · N satellites entendus** | Câblage OK, le module communique, pas encore de position | Aller dehors / près d'une fenêtre, attendre 1 à 5 min au premier démarrage |
| **Position acquise** | Tout fonctionne | — |

**Le tracé GPS bougeait tout seul, robot immobile ?** C'est normal pour un GPS : chaque mesure varie de 2 à 5 m dehors, et de 10 à 20 m en intérieur. Le robot filtre maintenant la position (`robot/gps_filtre.h`) :
- **moteurs arrêtés et vitesse GPS faible** : la position est figée puis moyennée, et le trajet ne bouge plus ;
- **mesures imprécises (HDOP > 8) et sauts aberrants** : ignorés ;
- **robot qui roule ou GPS porté à la main** : le trajet suit.

Dans l'onglet GPS, les points pâles montrent les mesures brutes (le bruit) ; la ligne est le trajet filtré.
