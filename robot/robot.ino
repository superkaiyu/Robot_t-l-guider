/*
  =================================================================================
  FIRMWARE ESP32 #2 — "ROBOT" — Roues + GPS + WiFi AP(+STA) + IHM intégrée
  =================================================================================

  Carte : ESP32-S3 N16R8 (DevKitC-1, 16 Mo de flash, 8 Mo de PSRAM).
  (L'ancienne carte Seeed XIAO ESP32S3 reste prise en charge : le brochage est choisi
  automatiquement selon la carte sélectionnée dans l'Arduino IDE, voir BROCHAGE.)

  Ce programme transforme la carte en cerveau de robot :
  1. Crée TOUJOURS son propre point d'accès WiFi "ROBOT_ESP32" (192.168.4.1).
     C'est le réseau commun sur lequel tout le monde communique :
        - l'ESP32 "Manette" (XIAO + joystick sur breadboard) s'y connecte,
        - le PC / téléphone (IHM) s'y connecte aussi.
     En option, il rejoint EN PLUS un réseau existant (voir secrets.h).
  2. DÉTECTE puis lit le module GPS branché en UART (fil TX du GPS -> GPIO14 ;
     d'autres broches libres et les vitesses courantes sont trouvées automatiquement),
     FILTRE la position (elle ne "bouge" plus quand le robot est à l'arrêt) et la
     diffuse en WiFi à l'IHM et à la Manette.
  3. Reçoit les commandes de déplacement (Manette et/ou IHM) et pilote 2 moteurs DC
     via un driver (TB6612FNG ou DRV8833 : 2 signaux PWM par moteur).
  4. Sert l'IHM directement : ouvrir http://192.168.4.1 dans un navigateur suffit
     (aucun fichier à ouvrir sur le PC, aucune connexion Internet nécessaire).
  5. Mesure la tension batterie (pont diviseur 30 kΩ / 10 kΩ) et le courant.

  BROCHAGE ESP32-S3 N16R8 (voir aussi le README) :
    Driver moteur : GPIO15 -> AIN1, GPIO16 -> AIN2 (roue gauche)
                    GPIO17 -> BIN1, GPIO18 -> BIN2 (roue droite)
                    TB6612 : VCC, STBY, PWMA, PWMB au 3V3 ; DRV8833 : nSLEEP au 3V3
                    GND du driver relié au GND de l'ESP32 (masse commune obligatoire)
    LED statut    : GPIO21 -> résistance 220-330 Ω -> LED -> GND
    Batterie      : GPIO1 = milieu du pont diviseur (30 kΩ vers le + batterie, 10 kΩ vers GND)
    Courant       : GPIO2 (relié au GND s'il n'y a pas de capteur)
    GPS           : TX du module -> GPIO14, VCC -> 3V3, GND -> GND (RX du module libre)
    USB-C         : port marqué "UART" (Moniteur Série + téléversement)

  ARCHITECTURE (tout passe par le WiFi du robot) :

      [GPS] --UART--> [ESP32 ROBOT] <--UDP 4210, 25x/s--> [ESP32 MANETTE + joystick]
                            ^
                            +--WebSocket port 81, 20x/s--> [PC/téléphone : IHM http://192.168.4.1]

  RÉACTIVITÉ : les deux liaisons restent ouvertes en permanence :
    - Manette -> Robot : datagrammes UDP (un paquet perdu est remplacé 40 ms plus tard).
    - Robot <-> IHM : WebSocket (le robot pousse la télémétrie, l'IHM envoie ses
      commandes dès qu'elles changent). Voir ws_serveur.h.
    Les routes HTTP (/cmd, /telemetrie) restent disponibles en secours.

  MONITEUR SÉRIE (115200 bauds) : "?" -> "ID=ROBOT GPS=..." (utilisé par tools/televerser.py
  pour trouver le bon port COM), "g" -> trames GPS brutes, "d" -> relancer la détection GPS.

  ⚠️ À PROPOS DU GPS : un module GPS calcule sa position en ÉCOUTANT les satellites.
  Sans ciel dégagé, pas de position. Le transport des données (GPS -> ESP32 -> IHM /
  Manette) passe uniquement par le fil série puis le WiFi de l'ESP32.
  Pour tester toute la chaîne en intérieur, passer GPS_SIMULATION à 1 ci-dessous.

  PRIORITÉ DES COMMANDES (plusieurs pilotes peuvent être connectés en même temps) :
    1. Arrêt d'urgence (bouton du joystick de la Manette, ou bouton de l'IHM)
    2. ESP32 Manette (dès que son joystick est écarté du centre)
    3. IHM (joystick virtuel, clavier, manette USB du PC)
    Une source au repos (commande 0,0) ne bloque jamais les autres.
    Sans commande fraîche depuis 500 ms -> moteurs coupés.

  Bibliothèque requise : "TinyGPSPlus" par Mikal Hart (Gestionnaire de bibliothèques).
  Arduino IDE, carte "ESP32S3 Dev Module" : USB CDC On Boot = Disabled (port UART),
  Flash Size = 16MB, PSRAM = OPI PSRAM. Téléversement : voir README (televerser_*.bat).
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <TinyGPSPlus.h>
#include <Preferences.h>
#include "gps_detection.h" // détection automatique de la broche et de la vitesse du GPS
#include "gps_filtre.h"    // filtre : la position ne "bouge" plus quand le robot est immobile
#include "ihm_html.h"    // page de l'IHM compressée (générée depuis ihm/ihm.html, voir README)
#include "ws_serveur.h"  // mini serveur WebSocket (aucune bibliothèque à installer)

const char* IDENTITE = "ROBOT"; // renvoyé sur le port série quand on tape "?"

// =========================================================
// OPTIONS
// =========================================================
// 1 = position GPS simulée (cercle de 10 m autour de SIM_LATITUDE/SIM_LONGITUDE),
//     pour vérifier toute la chaîne ESP32 -> WiFi -> IHM/Manette en intérieur.
// 0 = vraie lecture du module GPS Air530.
#define GPS_SIMULATION 0

// =========================================================
// CONFIGURATION RÉSEAU
// =========================================================
// --- Point d'accès (toujours actif) : c'est LE réseau commun Robot/Manette/IHM ---
const char* ap_ssid = "ROBOT_ESP32";
const char* ap_password = "motdepasse123"; // min. 8 caractères — doit être identique dans manette.ino
const int AP_CANAL_PAR_DEFAUT = 6;

// --- Réseaux externes optionnels (tentés en plus, dans l'ordre) ---
struct ReseauWifi {
  const char* nom;        // juste pour l'affichage dans le Moniteur Série
  const char* ssid;
  const char* motdepasse; // "" (chaîne vide) pour un réseau ouvert, comme WIFI-UP
};

// Les SSID / mots de passe personnels sont dans secrets.h (non versionné sur GitHub).
// Copier secrets_exemple.h en secrets.h et le compléter. Sans secrets.h, le robot
// fonctionne quand même, uniquement avec son point d'accès ROBOT_ESP32.
#if __has_include("secrets.h")
#include "secrets.h"
#else
ReseauWifi reseaux[] = { { "(aucun réseau externe)", "", "" } };
#endif
const int NB_RESEAUX = sizeof(reseaux) / sizeof(reseaux[0]);
const unsigned long TIMEOUT_CONNEXION_STA_MS = 15000;

WebServer server(80);
ServeurWebSocket ws(81);          // IHM : télémétrie poussée + commandes instantanées
WiFiUDP udp;                      // Manette : commandes à 25 Hz
const uint16_t PORT_UDP_ROBOT = 4210;
const unsigned long INTERVALLE_TELEMETRIE_WS_MS = 50; // 20 fois par seconde
unsigned long derniereTelemetrieWsMs = 0;

// =========================================================
// BROCHAGE — choisi automatiquement selon la carte sélectionnée dans l'Arduino IDE
// =========================================================
typedef DetecteurGps<HardwareSerial>::Broche BrocheGps;

#if defined(ARDUINO_XIAO_ESP32S3)
// ---------- Ancienne carte : Seeed XIAO ESP32S3 (carte "XIAO_ESP32S3") ----------
const char* NOM_CARTE = "XIAO ESP32S3";
const int PIN_ROUE_G_PWM_AV = 1;  // D0
const int PIN_ROUE_G_PWM_AR = 2;  // D1
const int PIN_ROUE_D_PWM_AV = 4;  // D3
const int PIN_ROUE_D_PWM_AR = 5;  // D4
const int PIN_LED_STATUT    = 6;  // D5
const int PIN_BATTERIE_ADC  = 7;  // D8
const int PIN_COURANT_ADC   = 8;  // D9
// GPS : D7 conseillé ; D6, D2, D10 essayées ensuite
const BrocheGps BROCHES_GPS[] = { { 44, "D7" }, { 43, "D6" }, { 3, "D2" }, { 9, "D10" } };

#else
// ---------- Carte du robot : ESP32-S3 N16R8 DevKitC-1 (carte "ESP32S3 Dev Module") ----------
const char* NOM_CARTE = "ESP32-S3 N16R8";
// Driver moteur (TB6612FNG ou DRV8833) : 2 signaux PWM par moteur
const int PIN_ROUE_G_PWM_AV = 15; // -> AIN1 (roue gauche, marche avant)
const int PIN_ROUE_G_PWM_AR = 16; // -> AIN2 (roue gauche, marche arrière)
const int PIN_ROUE_D_PWM_AV = 17; // -> BIN1 (roue droite, marche avant)
const int PIN_ROUE_D_PWM_AR = 18; // -> BIN2 (roue droite, marche arrière)
const int PIN_LED_STATUT    = 21; // LED + résistance 220-330 Ω vers GND
const int PIN_BATTERIE_ADC  = 1;  // milieu du pont diviseur 30 kΩ / 10 kΩ
const int PIN_COURANT_ADC   = 2;  // capteur de courant (relié au GND s'il n'y en a pas)
// GPS : TX du module -> GPIO14. Si rien n'arrive sur 14, le robot essaie aussi les broches
// libres voisines (fil planté une rangée à côté sur la breadboard...). Jamais 43/44 :
// ce sont les broches du port USB "UART" (Moniteur Série).
const BrocheGps BROCHES_GPS[] = {
  { 14, "GPIO14" }, { 13, "GPIO13" }, { 12, "GPIO12" }, { 11, "GPIO11" }, { 10, "GPIO10" },
  { 4, "GPIO4" }, { 5, "GPIO5" }, { 6, "GPIO6" }, { 7, "GPIO7" }
};
#endif

// --- Moteurs ---
// Une roue tourne à l'envers ? Passer son INVERSER_ à true (au lieu d'intervertir les fils).
const bool INVERSER_ROUE_G = false;
const bool INVERSER_ROUE_D = false;
// 20 kHz : au-delà de l'audible (plus de sifflement des moteurs). OK pour TB6612 et DRV8833.
// Avec un vieux L298N, mettre 1000.
const uint32_t FREQUENCE_PWM_MOTEURS = 20000;

// --- Mesure batterie : pont diviseur 30 kΩ (côté + batterie) / 10 kΩ (côté GND) ---
// Tension batterie = tension sur la broche x (30 + 10) / 10 = x 4. Pack 3S plein (12,6 V) -> 3,15 V.
const float RAPPORT_DIVISEUR_BATTERIE = (30.0 + 10.0) / 10.0;
const float TENSION_BATTERIE_ABSENTE = 2.0; // en dessous : pas de batterie branchée sur le pont
// --- Mesure courant : capteur analogique. Sans capteur, la broche est reliée au GND -> 0 A.
// Exemple ACS712-5A alimenté en 3V3 : offset ~1,65 V, ~0,185 V/A (à étalonner).
const float COURANT_OFFSET_V = 0.0;
const float COURANT_AMPERES_PAR_VOLT = 1.0;

// --- GPS ---
// On ne fait que LIRE la position : une seule broche de données suffit (TX du module).
// DÉTECTION AUTOMATIQUE (gps_detection.h) : broche (liste BROCHES_GPS ci-dessus) et
// vitesse (9600, 115200, 38400...) sont trouvées seules et mémorisées pour le démarrage suivant.
HardwareSerial gpsSerial(1); // UART matériel n°1 de l'ESP32, remappé sur la broche trouvée
TinyGPSPlus gps;
const unsigned long BAUDS_GPS[] = { 9600, 115200, 38400, 57600, 19200, 4800 };
DetecteurGps<HardwareSerial> detecteurGps(gpsSerial, BROCHES_GPS, sizeof(BROCHES_GPS) / sizeof(BROCHES_GPS[0]),
                                          BAUDS_GPS, sizeof(BAUDS_GPS) / sizeof(BAUDS_GPS[0]));
Preferences memoireGps;               // broche/vitesse trouvées, gardées en mémoire flash
bool detectionGpsVisible = true;      // false pendant les nouveaux essais silencieux (GPS absent)
bool gpsDernierResultatIllisible = false;
bool echoNmea = false;                // "g" dans le Moniteur Série : affiche les trames brutes du GPS
const unsigned long SILENCE_AVANT_REDETECTION_MS = 5000;
FiltreGps filtreGps;
unsigned long derniereCommandeMoteurMs = 0; // pour savoir si le robot roule (filtre GPS)
bool moteursDejaActifs = false;
const unsigned long INTERVALLE_REDETECTION_MS = 10000;
// Nombre de satellites VISIBLES (trames GSV), par constellation — utile en intérieur
// pour voir que le module "entend" quelques satellites même sans position.
TinyGPSCustom gsvGPS(gps, "GPGSV", 3);
TinyGPSCustom gsvBeidou(gps, "BDGSV", 3);
TinyGPSCustom gsvBeidou2(gps, "GBGSV", 3);
TinyGPSCustom gsvGlonass(gps, "GLGSV", 3);
TinyGPSCustom gsvGalileo(gps, "GAGSV", 3);

#if GPS_SIMULATION
const double SIM_LATITUDE = 48.858370;  // point de départ de la simulation, à adapter
const double SIM_LONGITUDE = 2.294481;
#endif

// =========================================================
// SÉCURITÉ ET PARAMÈTRES BATTERIE (Li-Ion 3S)
// =========================================================
const int NB_CELLULES_EQUIVALENT = 3;
const float SEUIL_SOUS_TENSION_PACK = 9.0;
const float SEUIL_SURTENSION_PACK = 12.6;
const float SEUIL_SURINTENSITE = 10.0;

// =========================================================
// VARIABLES GLOBALES
// =========================================================
int clignotementsRestants = 0;
unsigned long dernierToggleLedMs = 0;
const unsigned long INTERVALLE_CLIGNOTEMENT_MS = 150;
bool etatLed = false;

unsigned long dernierCalculMs = 0;
const unsigned long intervalleMesureMs = 150;

// --- Pilotage multi-sources ---
const unsigned long TIMEOUT_COMMANDE_MS = 500;       // commande plus vieille = ignorée
const unsigned long DUREE_ARRET_BOUTON_MS = 300;     // renouvelé tant que le bouton de la Manette est appuyé
const unsigned long DUREE_ARRET_IHM_MS = 1000;

struct CommandeSource {
  int rg = 0;
  int rd = 0;
  unsigned long ms = 0;
  bool recue = false;
};
CommandeSource cmdManette, cmdIhm;

enum SourcePilote { PILOTE_AUCUN, PILOTE_MANETTE, PILOTE_IHM, PILOTE_ARRET_URGENCE };
const char* NOMS_PILOTES[] = { "aucun", "manette", "ihm", "arret-urgence" };
SourcePilote piloteActuel = PILOTE_AUCUN;

unsigned long arretUrgenceDebutMs = 0;
unsigned long arretUrgenceDureeMs = 0;

// --- Présence des clients WiFi ---
const unsigned long TIMEOUT_CLIENT_MS = 2000;
unsigned long derniereRequeteManetteMs = 0;
unsigned long derniereRequeteIhmMs = 0;
bool manettePresente = false;
bool ihmPresente = false;
int manetteRssi = 0;

// --- Mesures ---
double consommationAh = 0.0;
int dernierPwrRoueG = 0, dernierPwrRoueD = 0;
float mesVitesseRoueG = 0, mesVitesseRoueD = 0;
float mesBatterie = 0, mesSoc = 0, mesCourant = 0;
const char* mesEtatBMS = "OK";
float distanceTotale = 0.0;

// Joystick physique affiché dans l'IHM : celui de la Manette (reçu en WiFi).
float mesJoystickAxeX = 0;
float mesJoystickAxeY = 0;
bool mesJoystickBouton = false;

// --- GPS ---
double mesLatitude = 0, mesLongitude = 0; // dernière position connue (conservée si le fix est perdu)
bool gpsFixValide = false;
bool gpsDejaFixe = false;
int gpsSatellites = 0;          // satellites utilisés pour le calcul
int gpsSatellitesVisibles = 0;  // satellites entendus (trames GSV)
float gpsAltitude = 0, gpsVitesseKmh = 0, gpsCap = 0, gpsHdop = 0;
char gpsHeureUTC[12] = "--:--:--";
unsigned long dernierCaractereGpsMs = 0;
const char* gpsEtat = "detection";  // detection | absent | illisible | recherche | fix | simulation

// --- WiFi externe (STA) ---
enum EtatSTA { STA_INACTIF, STA_CONNEXION, STA_CONNECTE, STA_ABANDON };
EtatSTA etatSta = STA_INACTIF;
int reseauStaIndex = -1;
unsigned long debutConnexionStaMs = 0;

// =========================================================
// LED
// =========================================================
void declencherClignotement(int nombreDeClignotements) {
  clignotementsRestants = nombreDeClignotements * 2;
  dernierToggleLedMs = millis();
}

void mettreAJourLed() {
  if (clignotementsRestants <= 0) return;
  if (millis() - dernierToggleLedMs >= INTERVALLE_CLIGNOTEMENT_MS) {
    etatLed = !etatLed;
    digitalWrite(PIN_LED_STATUT, etatLed ? HIGH : LOW);
    dernierToggleLedMs = millis();
    clignotementsRestants--;
    if (clignotementsRestants <= 0) digitalWrite(PIN_LED_STATUT, LOW);
  }
}

// =========================================================
// MOTEURS
// =========================================================
void appliquerMoteur(int pwr, int pinPwmAvant, int pinPwmArriere) {
  pwr = constrain(pwr, -100, 100);
  int pwm = map(abs(pwr), 0, 100, 0, 255);
  if (pwr > 0) {
    analogWrite(pinPwmAvant, pwm);
    analogWrite(pinPwmArriere, 0);
  } else if (pwr < 0) {
    analogWrite(pinPwmAvant, 0);
    analogWrite(pinPwmArriere, pwm);
  } else {
    analogWrite(pinPwmAvant, 0);
    analogWrite(pinPwmArriere, 0);
  }
}

void appliquerCommandes(int pwrRoueG, int pwrRoueD) {
  appliquerMoteur(INVERSER_ROUE_G ? -pwrRoueG : pwrRoueG, PIN_ROUE_G_PWM_AV, PIN_ROUE_G_PWM_AR);
  appliquerMoteur(INVERSER_ROUE_D ? -pwrRoueD : pwrRoueD, PIN_ROUE_D_PWM_AV, PIN_ROUE_D_PWM_AR);
  dernierPwrRoueG = pwrRoueG;
  dernierPwrRoueD = pwrRoueD;
  if (pwrRoueG != 0 || pwrRoueD != 0) {
    derniereCommandeMoteurMs = millis();
    moteursDejaActifs = true;
  }
}

// Vrai si les moteurs tournent ou tournaient il y a moins de 2 s (le robot peut encore rouler sur sa lancée)
bool robotEnMouvement() {
  return dernierPwrRoueG != 0 || dernierPwrRoueD != 0 || (moteursDejaActifs && millis() - derniereCommandeMoteurMs < 2000);
}

// =========================================================
// ARBITRAGE DES COMMANDES (Manette / IHM)
// =========================================================
void enregistrerCommande(CommandeSource& c, int rg, int rd) {
  c.rg = constrain(rg, -100, 100);
  c.rd = constrain(rd, -100, 100);
  c.ms = millis();
  c.recue = true;
}

bool commandeActive(const CommandeSource& c) {
  return c.recue && millis() - c.ms < TIMEOUT_COMMANDE_MS && (c.rg != 0 || c.rd != 0);
}

void declencherArretUrgence(unsigned long dureeMs) {
  arretUrgenceDebutMs = millis();
  arretUrgenceDureeMs = dureeMs;
}

bool arretUrgenceActif() {
  return arretUrgenceDureeMs > 0 && millis() - arretUrgenceDebutMs < arretUrgenceDureeMs;
}

// Appelée à chaque tour de loop() et à chaque commande reçue : choisit la source
// prioritaire encore "fraîche" et coupe les moteurs si plus personne ne pilote.
void mettreAJourPilotage() {
  SourcePilote source = PILOTE_AUCUN;
  int g = 0, d = 0;

  if (arretUrgenceActif()) {
    source = PILOTE_ARRET_URGENCE;
  } else if (commandeActive(cmdManette)) {
    source = PILOTE_MANETTE; g = cmdManette.rg; d = cmdManette.rd;
  } else if (commandeActive(cmdIhm)) {
    source = PILOTE_IHM; g = cmdIhm.rg; d = cmdIhm.rd;
  }

  if (source != piloteActuel) {
    piloteActuel = source;
    Serial.print("[Pilotage] Source active : ");
    Serial.println(NOMS_PILOTES[source]);
  }
  if (g != dernierPwrRoueG || d != dernierPwrRoueD) appliquerCommandes(g, d);
}

// =========================================================
// CAPTEURS ET TÉLÉMÉTRIE
// =========================================================
float socDepuisTensionCellule(float v) {
  const float points[][2] = {
    {3.00, 0}, {3.30, 5}, {3.50, 10}, {3.60, 20}, {3.65, 30},
    {3.70, 40}, {3.75, 50}, {3.80, 60}, {3.90, 70}, {4.00, 80},
    {4.10, 90}, {4.20, 100}
  };
  const int n = sizeof(points) / sizeof(points[0]);
  if (v <= points[0][0]) return 0;
  if (v >= points[n - 1][0]) return 100;
  for (int i = 0; i < n - 1; i++) {
    if (v >= points[i][0] && v <= points[i + 1][0]) {
      float v1 = points[i][0], s1 = points[i][1];
      float v2 = points[i + 1][0], s2 = points[i + 1][1];
      return s1 + (s2 - s1) * (v - v1) / (v2 - v1);
    }
  }
  return 0;
}

void rafraichirMesures(float deltaTSec) {
  // analogReadMilliVolts : tension corrigée par l'étalonnage d'usine de l'ADC (plus juste qu'analogRead)
  mesBatterie = analogReadMilliVolts(PIN_BATTERIE_ADC) / 1000.0 * RAPPORT_DIVISEUR_BATTERIE;
  mesCourant = (analogReadMilliVolts(PIN_COURANT_ADC) / 1000.0 - COURANT_OFFSET_V) * COURANT_AMPERES_PAR_VOLT;
  if (mesCourant < 0) mesCourant = 0;
  mesSoc = socDepuisTensionCellule(mesBatterie / NB_CELLULES_EQUIVALENT);

  mesEtatBMS = "OK";
  if (mesBatterie < TENSION_BATTERIE_ABSENTE) { mesEtatBMS = "ABSENTE"; mesSoc = 0; } // banc d'essai sans batterie
  else if (mesBatterie < SEUIL_SOUS_TENSION_PACK) mesEtatBMS = "SOUS-TENSION";
  else if (mesBatterie > SEUIL_SURTENSION_PACK) mesEtatBMS = "SURTENSION";
  else if (mesCourant > SEUIL_SURINTENSITE) mesEtatBMS = "SURINTENSITE";

  mesVitesseRoueG = 0.0; // pas de codeurs pour l'instant
  mesVitesseRoueD = 0.0;

  distanceTotale += fabs((mesVitesseRoueG + mesVitesseRoueD) / 2.0) * deltaTSec;
  consommationAh += mesCourant * deltaTSec / 3600.0;
}

// =========================================================
// GPS : LECTURE DE LA POSITION (Air530, trames NMEA)
// =========================================================
int lireSatellitesVisibles(TinyGPSCustom& champ) {
  if (!champ.isValid() || champ.age() > 5000) return 0;
  return atoi(champ.value());
}

String listeBrochesGps() {
  String l;
  for (size_t i = 0; i < sizeof(BROCHES_GPS) / sizeof(BROCHES_GPS[0]); i++) {
    if (i) l += ", ";
    l += BROCHES_GPS[i].nom;
  }
  return l;
}

String descriptionBrocheGps() {
  if (detecteurGps.etat() != DetecteurGps<HardwareSerial>::TROUVE) return "";
  String nom = detecteurGps.nomBroche(), gpio = "GPIO" + String(detecteurGps.gpio());
  return nom == gpio ? nom : nom + " (" + gpio + ")"; // "GPIO14" ou "D7 (GPIO44)"
}

void lancerDetectionGps(const char* raison, bool visible) {
  detectionGpsVisible = visible;
  if (visible) {
    Serial.printf("[GPS] Détection automatique du module (%s) : broches %s, vitesses 9600..115200 bauds...\n",
                  raison, listeBrochesGps().c_str());
  }
  // La dernière combinaison qui a marché est essayée en premier : détection quasi instantanée.
  detecteurGps.demarrer(memoireGps.getInt("gpio", -1), memoireGps.getULong("bauds", 0));
}

// À appeler à chaque tour de loop() (après lireGPS) : fait avancer la détection sans bloquer.
void gererDetectionGps() {
  static DetecteurGps<HardwareSerial>::Etat etatPrecedent = DetecteurGps<HardwareSerial>::EN_COURS;
  detecteurGps.mettreAJour();
  DetecteurGps<HardwareSerial>::Etat etat = detecteurGps.etat();

  if (detecteurGps.vientDeTrouver()) {
    Serial.printf("[GPS] ✅ Module GPS détecté sur %s à %lu bauds.\n", descriptionBrocheGps().c_str(), detecteurGps.vitesse());
    if (detecteurGps.gpio() != BROCHES_GPS[0].gpio) {
      Serial.printf("[GPS] (Ce n'est pas %s comme prévu, mais ça fonctionne : rien à changer.)\n", BROCHES_GPS[0].nom);
    }
    if (memoireGps.getInt("gpio", -1) != detecteurGps.gpio() || memoireGps.getULong("bauds", 0) != detecteurGps.vitesse()) {
      memoireGps.putInt("gpio", detecteurGps.gpio());
      memoireGps.putULong("bauds", detecteurGps.vitesse());
    }
    gpsDernierResultatIllisible = false;
    declencherClignotement(3);
  }

  if (etat == DetecteurGps<HardwareSerial>::INTROUVABLE && etatPrecedent != DetecteurGps<HardwareSerial>::INTROUVABLE) {
    gpsDernierResultatIllisible = detecteurGps.illisible();
    if (detectionGpsVisible) {
      if (gpsDernierResultatIllisible) {
        Serial.println("[GPS] ⚠️ Des données arrivent mais ne sont pas des trames GPS valides : vérifier le fil GND commun "
                       "et la vitesse du module. Nouvel essai toutes les 10 s.");
      } else {
        Serial.printf("[GPS] ❌ Aucun GPS trouvé (broches %s, toutes vitesses). Vérifier : fil TX du GPS -> %s, "
                      "VCC -> 3V3 (ou 5V selon le module), GND -> GND. Nouvel essai toutes les 10 s.\n",
                      listeBrochesGps().c_str(), BROCHES_GPS[0].nom);
      }
    }
  }
  etatPrecedent = etat;

  if (etat == DetecteurGps<HardwareSerial>::TROUVE && millis() - dernierCaractereGpsMs > SILENCE_AVANT_REDETECTION_MS) {
    Serial.println("[GPS] Le module ne répond plus (débranché ?) : nouvelle recherche...");
    lancerDetectionGps("GPS muet", false);
  } else if (etat == DetecteurGps<HardwareSerial>::INTROUVABLE && detecteurGps.depuisFinMs() > INTERVALLE_REDETECTION_MS) {
    lancerDetectionGps("nouvel essai", false); // silencieux : le GPS peut être branché à chaud
  }
}

// À appeler à chaque tour de loop() : fait avancer le décodage NMEA sans jamais bloquer.
void lireGPS() {
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gps.encode(c);
    detecteurGps.caractereRecu(c);
    dernierCaractereGpsMs = millis();
    if (echoNmea && detecteurGps.etat() == DetecteurGps<HardwareSerial>::TROUVE) Serial.write(c);
  }

#if GPS_SIMULATION
  float angle = (millis() % 60000UL) / 60000.0 * 2 * PI; // un tour de cercle par minute
  mesLatitude = SIM_LATITUDE + (10.0 * sin(angle)) / 110540.0;
  mesLongitude = SIM_LONGITUDE + (10.0 * cos(angle)) / (111320.0 * cos(SIM_LATITUDE * PI / 180.0));
  gpsFixValide = true;
  gpsDejaFixe = true;
  gpsSatellites = 8;
  gpsSatellitesVisibles = 12;
  gpsAltitude = 35.0;
  gpsVitesseKmh = 3.8;
  gpsCap = fmod(angle * 180.0 / PI + 270.0, 360.0);
  gpsHdop = 0.9;
  gpsEtat = "simulation";
#else
  bool trouve = detecteurGps.etat() == DetecteurGps<HardwareSerial>::TROUVE;
  bool moduleQuiParle = trouve && millis() - dernierCaractereGpsMs < 2000;

  bool fixBrut = moduleQuiParle && gps.location.isValid() && gps.location.age() < 3000;
  if (fixBrut && gps.location.isUpdated()) { // nouvelle mesure (1 par seconde en général) -> filtre
    filtreGps.ajouter(gps.location.lat(), gps.location.lng(), gps.hdop.isValid() ? gps.hdop.hdop() : 99.0f,
                      gps.satellites.isValid() ? (int)gps.satellites.value() : 0,
                      gps.speed.isValid() ? gps.speed.kmph() : 0.0f, robotEnMouvement());
  }
  gpsFixValide = fixBrut && filtreGps.pret();
  if (gpsFixValide) {
    mesLatitude = filtreGps.lat(); // position FILTRÉE : c'est elle qu'on envoie à l'IHM et à la Manette
    mesLongitude = filtreGps.lon();
    gpsDejaFixe = true;
  }
  gpsSatellites = (gps.satellites.isValid() && gps.satellites.age() < 5000) ? gps.satellites.value() : 0;
  gpsSatellitesVisibles = lireSatellitesVisibles(gsvGPS) + lireSatellitesVisibles(gsvBeidou) + lireSatellitesVisibles(gsvBeidou2)
                        + lireSatellitesVisibles(gsvGlonass) + lireSatellitesVisibles(gsvGalileo);
  if (gpsSatellitesVisibles < gpsSatellites) gpsSatellitesVisibles = gpsSatellites; // trames GSV d'un type non reconnu
  if (gps.altitude.isValid()) gpsAltitude = gps.altitude.meters();
  if (gps.speed.isValid()) gpsVitesseKmh = gps.speed.kmph();
  if (gps.course.isValid()) gpsCap = gps.course.deg();
  if (gps.hdop.isValid()) gpsHdop = gps.hdop.hdop();
  if (gps.time.isValid()) {
    snprintf(gpsHeureUTC, sizeof(gpsHeureUTC), "%02d:%02d:%02d", gps.time.hour(), gps.time.minute(), gps.time.second());
  }

  if (detecteurGps.etat() == DetecteurGps<HardwareSerial>::EN_COURS && detectionGpsVisible) gpsEtat = "detection";
  else if (!trouve) gpsEtat = gpsDernierResultatIllisible ? "illisible" : "absent";
  else if (!moduleQuiParle) gpsEtat = "absent";
  else if (gpsFixValide) gpsEtat = "fix";
  else gpsEtat = "recherche";
#endif
}

void afficherEtatGpsSerie() {
  static unsigned long dernierAffichageMs = 0;
  if (millis() - dernierAffichageMs < 5000) return;
  dernierAffichageMs = millis();
  if (strcmp(gpsEtat, "detection") == 0) return; // la détection affiche ses propres messages
  Serial.printf("[GPS] etat=%s", gpsEtat);
  if (detecteurGps.etat() == DetecteurGps<HardwareSerial>::TROUVE) {
    Serial.printf("  branché sur %s à %lu bauds", descriptionBrocheGps().c_str(), detecteurGps.vitesse());
  }
  Serial.printf("  trames OK/KO=%lu/%lu  satellites utilises/visibles=%d/%d",
                (unsigned long)gps.passedChecksum(), (unsigned long)gps.failedChecksum(), gpsSatellites, gpsSatellitesVisibles);
  if (gpsFixValide) Serial.printf("  position=%.6f, %.6f", mesLatitude, mesLongitude);
  Serial.println();
}

// =========================================================
// PRÉSENCE DES CLIENTS (Manette / IHM)
// =========================================================
void surveillerClients() {
  if (manettePresente && millis() - derniereRequeteManetteMs > TIMEOUT_CLIENT_MS) {
    manettePresente = false;
    mesJoystickAxeX = 0; mesJoystickAxeY = 0; mesJoystickBouton = false;
    Serial.println("[WiFi] ESP32 Manette déconnectée.");
    declencherClignotement(2);
  }
  if (ihmPresente && millis() - derniereRequeteIhmMs > TIMEOUT_CLIENT_MS) {
    ihmPresente = false;
    Serial.println("[WiFi] IHM (navigateur) déconnectée.");
  }
}

void noterRequeteManette() {
  derniereRequeteManetteMs = millis();
  if (!manettePresente) {
    manettePresente = true;
    Serial.println("[WiFi] ESP32 Manette connectée !");
    declencherClignotement(5);
  }
}

void noterRequeteIhm() {
  derniereRequeteIhmMs = millis();
  if (!ihmPresente) {
    ihmPresente = true;
    Serial.println("[WiFi] IHM (navigateur) connectée.");
    declencherClignotement(3);
  }
}

// =========================================================
// SERVEUR HTTP
// =========================================================
void ajouterEnTetesCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
}

void jsonChamp(String& s, const char* cle, double valeur, int decimales = 3) {
  s += "\""; s += cle; s += "\":"; s += String(valeur, decimales); s += ",";
}
void jsonChampInt(String& s, const char* cle, long valeur) {
  s += "\""; s += cle; s += "\":"; s += String(valeur); s += ",";
}
void jsonChampTexte(String& s, const char* cle, const String& valeur) {
  s += "\""; s += cle; s += "\":\"";
  for (size_t i = 0; i < valeur.length(); i++) {
    char c = valeur[i];
    if (c == '"' || c == '\\') s += '\\';
    if ((unsigned char)c >= 0x20) s += c;
  }
  s += "\",";
}
void jsonChampBool(String& s, const char* cle, bool valeur) {
  s += "\""; s += cle; s += "\":"; s += (valeur ? "true" : "false"); s += ",";
}

void ajouterChampsGPS(String& t) {
  jsonChampTexte(t, "gpsEtat", gpsEtat);
  jsonChampBool(t, "gpsFix", gpsFixValide);
  jsonChampBool(t, "gpsDejaFixe", gpsDejaFixe);
  jsonChamp(t, "gpsLat", mesLatitude, 6);
  jsonChamp(t, "gpsLon", mesLongitude, 6);
  jsonChamp(t, "gpsLatBrute", filtreGps.pret() ? filtreGps.latitudeBrute() : mesLatitude, 6);
  jsonChamp(t, "gpsLonBrute", filtreGps.pret() ? filtreGps.longitudeBrute() : mesLongitude, 6);
  jsonChampBool(t, "gpsImmobile", filtreGps.immobile());
  jsonChampInt(t, "gpsMoyenne", filtreGps.mesuresMoyennees());
  jsonChampBool(t, "gpsFige", filtreGps.fige());
  jsonChamp(t, "gpsPrecision", filtreGps.pret() ? filtreGps.precisionM() : 0, 1);
  jsonChampInt(t, "gpsSatellites", gpsSatellites);
  jsonChampInt(t, "gpsSatellitesVisibles", gpsSatellitesVisibles);
  jsonChamp(t, "gpsAltitude", gpsAltitude, 1);
  jsonChamp(t, "gpsVitesseKmh", gpsVitesseKmh, 1);
  jsonChamp(t, "gpsCap", gpsCap, 0);
  jsonChamp(t, "gpsHdop", gpsHdop, 2);
  jsonChampTexte(t, "gpsHeureUTC", gpsHeureUTC);
  jsonChampInt(t, "gpsCaracteres", (long)gps.charsProcessed());
  jsonChampInt(t, "gpsTramesOk", (long)gps.passedChecksum());
  jsonChampInt(t, "gpsTramesKo", (long)gps.failedChecksum());
  jsonChampTexte(t, "gpsBroche", descriptionBrocheGps());
  jsonChampInt(t, "gpsBauds", (long)detecteurGps.vitesse());
  jsonChampTexte(t, "gpsBrochesTestees", listeBrochesGps());
  char essai[48] = "";
  if (detecteurGps.etat() == DetecteurGps<HardwareSerial>::EN_COURS) detecteurGps.decrireEssai(essai, sizeof(essai));
  jsonChampTexte(t, "gpsEssai", essai);
}

// Commande reçue de l'ESP32 Manette (par UDP, ou par HTTP pour les anciennes versions)
void traiterCommandeManette(int rg, int rd, float jx, float jy, bool bouton, int rssi) {
  noterRequeteManette();
  enregistrerCommande(cmdManette, rg, rd);
  mesJoystickAxeX = constrain(jx, -1.0f, 1.0f);
  mesJoystickAxeY = constrain(jy, -1.0f, 1.0f);
  mesJoystickBouton = bouton;
  if (rssi != 0) manetteRssi = rssi;
  if (bouton) declencherArretUrgence(DUREE_ARRET_BOUTON_MS);
  mettreAJourPilotage();
}

// Réponse renvoyée à la Manette : OK;pilote=...;gps=...;fix=0|1;lat=...;lon=...;sat=...
String construireReponseManette() {
  String r = "OK;pilote=";
  r += NOMS_PILOTES[piloteActuel];
  r += ";gps="; r += gpsEtat;
  r += ";fix="; r += (gpsFixValide ? "1" : "0");
  r += ";lat="; r += String(mesLatitude, 6);
  r += ";lon="; r += String(mesLongitude, 6);
  r += ";sat="; r += String(gpsSatellites);
  return r;
}

void construireTelemetrie(String& t) {
  t = "{";
  t.reserve(1200);
  jsonChamp(t, "vitesseRoueG", mesVitesseRoueG);
  jsonChamp(t, "vitesseRoueD", mesVitesseRoueD);
  jsonChamp(t, "distance", distanceTotale);
  jsonChamp(t, "batterie", mesBatterie);
  jsonChamp(t, "soc", mesSoc, 1);
  jsonChampTexte(t, "etatBMS", mesEtatBMS);
  jsonChamp(t, "courant", mesCourant);
  jsonChamp(t, "consommation", consommationAh, 4);
  jsonChampInt(t, "pwrRoueG", dernierPwrRoueG);
  jsonChampInt(t, "pwrRoueD", dernierPwrRoueD);
  jsonChampTexte(t, "pilote", NOMS_PILOTES[piloteActuel]);
  jsonChamp(t, "joystickX", mesJoystickAxeX);
  jsonChamp(t, "joystickY", mesJoystickAxeY);
  jsonChampBool(t, "joystickBouton", mesJoystickBouton);
  jsonChampBool(t, "manetteConnectee", manettePresente);
  jsonChampInt(t, "manetteRssi", manetteRssi);
  ajouterChampsGPS(t);
  jsonChampInt(t, "apClients", WiFi.softAPgetStationNum());
  jsonChampInt(t, "ihmClients", ws.nbClients());
  jsonChampBool(t, "staConnecte", etatSta == STA_CONNECTE);
  jsonChampTexte(t, "staSsid", etatSta == STA_CONNECTE ? WiFi.SSID() : String(""));
  jsonChampTexte(t, "staIp", etatSta == STA_CONNECTE ? WiFi.localIP().toString() : String(""));
  jsonChampInt(t, "uptime", (long)(millis() / 1000));
  jsonChampTexte(t, "carte", NOM_CARTE);
  t.remove(t.length() - 1);
  t += "}";
}

// /cmd?src=manette|ihm&rg=-100..100&rd=-100..100[&stop=1][&jx=..&jy=..&jb=0|1&rssi=..]
// Route de secours : l'IHM passe normalement par le WebSocket et la Manette par UDP.
void handleCmd() {
  String src = server.arg("src");
  int rg = server.arg("rg").toInt();
  int rd = server.arg("rd").toInt();

  if (src == "manette") {
    traiterCommandeManette(rg, rd, server.arg("jx").toFloat(), server.arg("jy").toFloat(),
                           server.arg("jb") == "1", server.arg("rssi").toInt());
  } else {
    // Sans "src" : compatibilité avec les anciennes versions de l'IHM
    noterRequeteIhm();
    enregistrerCommande(cmdIhm, rg, rd);
  }
  if (server.arg("stop") == "1") declencherArretUrgence(DUREE_ARRET_IHM_MS);
  mettreAJourPilotage();

  ajouterEnTetesCORS();
  server.send(200, "text/plain", construireReponseManette());
}

void handleTelemetrie() {
  noterRequeteIhm();
  String t;
  construireTelemetrie(t);
  ajouterEnTetesCORS();
  server.send(200, "application/json", t);
}

// =========================================================
// UDP : LIAISON RAPIDE AVEC L'ESP32 MANETTE
// =========================================================
// Paquet reçu : "M,<rg>,<rd>,<jx>,<jy>,<bouton 0|1>,<rssi>"  -> réponse : construireReponseManette()
void lireUdpManette() {
  for (int paquets = 0; paquets < 4; paquets++) { // vide la file sans monopoliser la boucle
    int taille = udp.parsePacket();
    if (taille <= 0) return;
    char tampon[96];
    int lu = udp.read((uint8_t*)tampon, sizeof(tampon) - 1);
    if (lu <= 0) continue;
    tampon[lu] = 0;
    if (tampon[0] != 'M' || tampon[1] != ',') continue;

    float champs[6] = { 0, 0, 0, 0, 0, 0 };
    int n = 0;
    char* suite = nullptr;
    for (char* morceau = strtok_r(tampon + 2, ",", &suite); morceau && n < 6; morceau = strtok_r(nullptr, ",", &suite)) {
      champs[n++] = atof(morceau);
    }
    if (n < 5) continue;
    traiterCommandeManette((int)champs[0], (int)champs[1], champs[2], champs[3], champs[4] != 0, (int)champs[5]);

    String r = construireReponseManette();
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write((const uint8_t*)r.c_str(), r.length());
    udp.endPacket();
  }
}

// =========================================================
// WEBSOCKET : LIAISON TEMPS RÉEL AVEC L'IHM
// =========================================================
// Messages de l'IHM : "c,<rg>,<rd>" (commande), "stop" (arrêt d'urgence), "p,<n>" (mesure de latence)
void surMessageWs(uint8_t client, char* message, size_t longueur) {
  noterRequeteIhm();
  if (message[0] == 'c' && message[1] == ',') {
    char* suite = nullptr;
    int rg = atoi(message + 2);
    strtok_r(message + 2, ",", &suite);
    int rd = suite ? atoi(suite) : 0;
    enregistrerCommande(cmdIhm, rg, rd);
    mettreAJourPilotage();
  } else if (strcmp(message, "gpsdetect") == 0) {
    lancerDetectionGps("demandée depuis l'IHM", true);
  } else if (strcmp(message, "stop") == 0) {
    enregistrerCommande(cmdIhm, 0, 0);
    declencherArretUrgence(DUREE_ARRET_IHM_MS);
    mettreAJourPilotage();
  } else if (message[0] == 'p' && message[1] == ',') {
    message[0] = 'P'; // renvoyé tel quel : l'IHM mesure l'aller-retour
    ws.envoyer(client, message, longueur);
  }
}

void surConnexionWs(uint8_t client, bool connecte) {
  Serial.printf("[WebSocket] IHM n°%d %s (%d connectée(s))\n", client, connecte ? "connectée" : "déconnectée", ws.nbClients());
  if (connecte) {
    noterRequeteIhm();
    derniereTelemetrieWsMs = 0; // envoie tout de suite un premier état complet
  }
}

void diffuserTelemetrieWs() {
  if (ws.nbClients() == 0 || millis() - derniereTelemetrieWsMs < INTERVALLE_TELEMETRIE_WS_MS) return;
  derniereTelemetrieWsMs = millis();
  String t;
  construireTelemetrie(t);
  ws.diffuser(t.c_str(), t.length());
}

// =========================================================
// IDENTIFICATION SUR LE PORT USB
// =========================================================
// Commandes du Moniteur Série :  ?  identité + état du GPS   g  trames GPS brutes (marche/arrêt)
//                                 d  relancer la détection du GPS
void repondreIdentification() {
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c == '?') {
      Serial.printf("ID=%s GPS=%s,%s,%lu\n", IDENTITE, gpsEtat,
                    detecteurGps.etat() == DetecteurGps<HardwareSerial>::TROUVE ? detecteurGps.nomBroche() : "-", detecteurGps.vitesse());
    } else if (c == 'g') {
      echoNmea = !echoNmea;
      Serial.println(echoNmea ? "[GPS] Affichage des trames brutes : MARCHE (g pour arrêter)" : "[GPS] Affichage des trames brutes : ARRÊT");
    } else if (c == 'd') {
      lancerDetectionGps("demandée depuis le Moniteur Série", true);
    }
  }
}

// Relance la détection du GPS (bouton de l'IHM en mode HTTP de secours)
void handleGpsDetecter() {
  lancerDetectionGps("demandée depuis l'IHM", true);
  ajouterEnTetesCORS();
  server.send(200, "text/plain", "OK");
}

// Position GPS seule (pratique pour un téléphone ou un autre appareil)
void handleGps() {
  String t = "{";
  ajouterChampsGPS(t);
  t.remove(t.length() - 1);
  t += "}";
  ajouterEnTetesCORS();
  server.send(200, "application/json", t);
}

// =========================================================
// FENÊTRE DE CONNEXION AU PORTAIL CAPTIF WIFI-UP
// =========================================================
// Accessible depuis un navigateur : http://<IP de l'ESP32>/wifiup
// Utile uniquement si l'ESP32 a réussi à rejoindre WIFI-UP en STA.
//
// ⚠️ À COMPLÉTER avant que ça fonctionne réellement : remplace URL_PORTAIL,
// CHAMP_IDENTIFIANT et CHAMP_MOTDEPASSE ci-dessous par les valeurs exactes du
// portail de l'IUT (Outils de développement du navigateur, onglet Réseau,
// pendant une connexion normale au portail).
const char* URL_PORTAIL = "http://URL_DU_PORTAIL_A_COMPLETER/login";
const char* CHAMP_IDENTIFIANT = "username"; // À VÉRIFIER
const char* CHAMP_MOTDEPASSE  = "password"; // À VÉRIFIER

String encoderUrl(const String& texte) {
  String r;
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < texte.length(); i++) {
    char c = texte[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      r += c;
    } else {
      r += '%';
      r += hex[((unsigned char)c) >> 4];
      r += hex[((unsigned char)c) & 0x0F];
    }
  }
  return r;
}

void handleWifiUpForm() {
  String page = R"HTML(
    <!DOCTYPE html><html><head><meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Connexion WIFI-UP</title></head>
    <body style="font-family:sans-serif;background:#1e1e2f;color:#fff;padding:30px;max-width:400px;margin:0 auto;">
      <h2 style="color:#4CAF50;">Connexion au portail WIFI-UP</h2>
      <p style="font-size:0.85em;color:#999;">Ces identifiants ne sont utilisés que pour authentifier l'ESP32 sur le portail — rien n'est envoyé ailleurs.</p>
      <form method="POST" action="/wifiup-login">
        <label>Identifiant étudiant :</label><br>
        <input type="text" name="identifiant" style="padding:8px;width:100%;box-sizing:border-box;margin:6px 0 16px 0;" required><br>
        <label>Mot de passe :</label><br>
        <input type="password" name="motdepasse" style="padding:8px;width:100%;box-sizing:border-box;margin:6px 0 16px 0;" required><br>
        <button type="submit" style="padding:10px 20px;background:#4CAF50;color:#fff;border:none;border-radius:5px;cursor:pointer;width:100%;">Se connecter</button>
      </form>
    </body></html>
  )HTML";
  ajouterEnTetesCORS();
  server.send(200, "text/html", page);
}

void handleWifiUpLogin() {
  String identifiant = server.arg("identifiant");
  String motdepasse = server.arg("motdepasse");

  Serial.print("[WIFI-UP] Tentative de connexion au portail avec l'identifiant : ");
  Serial.println(identifiant);

  // Les requêtes ci-dessous bloquent la boucle quelques secondes : on arrête le robot avant.
  appliquerCommandes(0, 0);

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(4000);
  http.begin(URL_PORTAIL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String corps = String(CHAMP_IDENTIFIANT) + "=" + encoderUrl(identifiant) + "&" + String(CHAMP_MOTDEPASSE) + "=" + encoderUrl(motdepasse);
  int code = http.POST(corps);
  http.end();

  bool internetOk = false;
  {
    HTTPClient verif;
    verif.setConnectTimeout(3000);
    verif.setTimeout(4000);
    verif.begin("http://connectivitycheck.gstatic.com/generate_204");
    internetOk = (verif.GET() == 204);
    verif.end();
  }

  String reponse = "Requête envoyée au portail (code HTTP " + String(code) + ").<br>";
  reponse += internetOk ? "✅ Accès Internet détecté, ça a fonctionné !" : "⚠️ Toujours pas d'accès Internet détecté — vérifie l'URL/les noms de champs en haut du fichier (voir commentaire À COMPLÉTER).";

  ajouterEnTetesCORS();
  server.send(200, "text/html", "<meta charset='UTF-8'><body style='font-family:sans-serif;background:#1e1e2f;color:#fff;padding:30px;'>" + reponse + "<br><br><a href='/wifiup' style='color:#4CAF50;'>Retour</a></body>");
}

// L'IHM complète est servie par le robot lui-même : http://192.168.4.1
// (compressée en gzip : ~4 fois moins de données -> page chargée plus vite)
void handleRacine() {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html; charset=utf-8", (const char*)IHM_HTML_GZ, IHM_HTML_GZ_TAILLE);
}

void handleAide() {
  ajouterEnTetesCORS();
  server.send(200, "text/plain; charset=utf-8",
              "Robot ESP32 OK. Routes : /  (IHM)   /cmd?src=ihm&rg=0&rd=0   /telemetrie   /gps   /gps-detecter   /wifiup (portail WIFI-UP)\n"
              "Temps réel : WebSocket ws://<ip>:81/  et  UDP port 4210 (Manette)");
}

void handleNotFound() {
  ajouterEnTetesCORS();
  server.send(404, "text/plain", "Route inconnue");
}

// =========================================================
// WIFI : POINT D'ACCÈS + RÉSEAU EXTERNE OPTIONNEL
// =========================================================
// Un ESP32 n'a qu'une seule radio : le point d'accès et la connexion externe
// partagent le même canal. On cherche donc d'abord quel réseau connu est à
// portée, puis on crée le point d'accès SUR SON CANAL — ainsi la Manette n'est
// jamais déconnectée par un changement de canal.
int choisirReseauExterne(int& canal) {
  bool auMoinsUnReseau = false;
  for (int i = 0; i < NB_RESEAUX; i++) {
    if (reseaux[i].ssid && reseaux[i].ssid[0]) auMoinsUnReseau = true;
  }
  if (!auMoinsUnReseau) {
    Serial.println("[WiFi] Aucun réseau externe configuré (secrets.h) : point d'accès seul.");
    return -1;
  }

  Serial.println("[WiFi] Recherche des réseaux externes connus...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < NB_RESEAUX; i++) {
    if (!reseaux[i].ssid || !reseaux[i].ssid[0]) continue;
    for (int j = 0; j < n; j++) {
      if (WiFi.SSID(j) == reseaux[i].ssid) {
        canal = WiFi.channel(j);
        WiFi.scanDelete();
        return i;
      }
    }
  }
  WiFi.scanDelete();
  Serial.println("[WiFi] Aucun réseau externe connu à portée : point d'accès seul.");
  return -1;
}

void demarrerWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);          // latence minimale pour les commandes
  WiFi.setAutoReconnect(false);  // pas de balayage en boucle qui perturberait le point d'accès

  int canal = AP_CANAL_PAR_DEFAUT;
  reseauStaIndex = choisirReseauExterne(canal);

  WiFi.softAP(ap_ssid, ap_password, canal);
  Serial.print("[WiFi] Point d'accès créé : ");
  Serial.print(ap_ssid);
  Serial.print(" (canal ");
  Serial.print(canal);
  Serial.print(") — IHM : http://");
  Serial.println(WiFi.softAPIP());

  if (reseauStaIndex >= 0) {
    const ReseauWifi& r = reseaux[reseauStaIndex];
    Serial.print("[WiFi] Connexion en plus à : ");
    Serial.print(r.nom);
    Serial.print(" (");
    Serial.print(r.ssid);
    Serial.println(")...");
    if (strlen(r.motdepasse) == 0) WiFi.begin(r.ssid); // réseau ouvert (ex : WIFI-UP)
    else WiFi.begin(r.ssid, r.motdepasse);
    etatSta = STA_CONNEXION;
    debutConnexionStaMs = millis();
  }
}

// Non bloquant : le robot reste pilotable pendant la connexion au réseau externe.
void gererWifiExterne() {
  if (etatSta == STA_CONNEXION) {
    if (WiFi.status() == WL_CONNECTED) {
      etatSta = STA_CONNECTE;
      Serial.print("[WiFi] Également connecté à ");
      Serial.print(reseaux[reseauStaIndex].nom);
      Serial.print(" — IHM aussi accessible sur http://");
      Serial.println(WiFi.localIP());
    } else if (millis() - debutConnexionStaMs > TIMEOUT_CONNEXION_STA_MS) {
      WiFi.disconnect();
      etatSta = STA_ABANDON;
      Serial.println("[WiFi] Échec de connexion au réseau externe — le point d'accès ROBOT_ESP32 reste disponible.");
    }
  } else if (etatSta == STA_CONNECTE && WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    etatSta = STA_ABANDON;
    Serial.println("[WiFi] Réseau externe perdu — le point d'accès ROBOT_ESP32 reste disponible.");
  }
}

// =========================================================
// SETUP
// =========================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.print("ID=");
  Serial.println(IDENTITE);
  Serial.printf("Carte : %s — moteurs GPIO %d/%d (gauche) %d/%d (droite), LED GPIO %d, batterie GPIO %d, GPS %s\n",
                NOM_CARTE, PIN_ROUE_G_PWM_AV, PIN_ROUE_G_PWM_AR, PIN_ROUE_D_PWM_AV, PIN_ROUE_D_PWM_AR,
                PIN_LED_STATUT, PIN_BATTERIE_ADC, BROCHES_GPS[0].nom);

  pinMode(PIN_ROUE_G_PWM_AV, OUTPUT);
  pinMode(PIN_ROUE_G_PWM_AR, OUTPUT);
  pinMode(PIN_ROUE_D_PWM_AV, OUTPUT);
  pinMode(PIN_ROUE_D_PWM_AR, OUTPUT);
  pinMode(PIN_LED_STATUT, OUTPUT);
  appliquerCommandes(0, 0); // attache aussi les broches au PWM
  for (int pin : { PIN_ROUE_G_PWM_AV, PIN_ROUE_G_PWM_AR, PIN_ROUE_D_PWM_AV, PIN_ROUE_D_PWM_AR }) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    analogWriteFrequency(pin, FREQUENCE_PWM_MOTEURS);
#else
    (void)pin;
    analogWriteFrequency(FREQUENCE_PWM_MOTEURS);
#endif
  }

  Serial.println("Commandes série : ? = identité + GPS, g = trames GPS brutes, d = relancer la détection du GPS");
  memoireGps.begin("gps", false);
  lancerDetectionGps("démarrage", true); // ouvre l'UART du GPS sur la broche/vitesse à essayer

  demarrerWifi();

  server.on("/", handleRacine);
  server.on("/aide", handleAide);
  server.on("/cmd", handleCmd);
  server.on("/telemetrie", handleTelemetrie);
  server.on("/gps", handleGps);
  server.on("/gps-detecter", handleGpsDetecter);
  server.on("/wifiup", HTTP_GET, handleWifiUpForm);
  server.on("/wifiup-login", HTTP_POST, handleWifiUpLogin);
  server.onNotFound(handleNotFound);
  server.begin();
  ws.surMessage(surMessageWs);
  ws.surConnexion(surConnexionWs);
  ws.begin();
  udp.begin(PORT_UDP_ROBOT);
  Serial.println("Serveurs démarrés : HTTP 80 (IHM), WebSocket 81 (temps réel), UDP 4210 (Manette). Robot prêt !");
#if GPS_SIMULATION
  Serial.println("[GPS] ⚠️ MODE SIMULATION ACTIF (GPS_SIMULATION = 1)");
#endif

  dernierCalculMs = millis();
}

// =========================================================
// LOOP
// =========================================================
void loop() {
  server.handleClient();
  ws.loop();
  lireUdpManette();
  lireGPS();
  gererDetectionGps();
  mettreAJourLed();
  mettreAJourPilotage(); // coupe aussi les moteurs si plus aucune commande fraîche
  surveillerClients();
  gererWifiExterne();
  afficherEtatGpsSerie();
  diffuserTelemetrieWs();
  repondreIdentification();

  if (millis() - dernierCalculMs > intervalleMesureMs) {
    float deltaTSec = (millis() - dernierCalculMs) / 1000.0;
    dernierCalculMs = millis();
    rafraichirMesures(deltaTSec);
  }
}
