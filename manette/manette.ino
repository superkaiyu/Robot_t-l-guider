/*
  =================================================================================
  FIRMWARE ESP32 #1 — "MANETTE" — Lit le joystick, envoie les ordres au robot,
                                   reçoit la position GPS du robot
  =================================================================================

  Architecture à 2 ESP32 (tout passe par le WiFi créé par le robot) :
    - Cet ESP32 (la "Manette") lit un joystick physique sur breadboard et envoie
      les commandes de déplacement + la position du stick au robot, en WiFi.
    - L'autre ESP32 (le "Robot") pilote les moteurs, lit le GPS, et renvoie la
      position GPS dans CHAQUE réponse -> la Manette l'affiche dans son Moniteur Série.
    - Le robot relaie aussi la position du stick à l'IHM (http://192.168.4.1).

  Échange 25 fois par seconde, en UDP (le plus réactif : pas de connexion à
  ouvrir, un paquet perdu est remplacé 40 ms plus tard par le suivant) :
    Manette -> Robot (port 4210) : M,<rg>,<rd>,<jx>,<jy>,<bouton 0|1>,<rssi>
    Robot -> Manette             : OK;pilote=manette;gps=fix;fix=1;lat=48.858370;lon=2.294481;sat=8

  Réseau à rejoindre (créé par le robot) :
    SSID : ROBOT_ESP32
    Mot de passe : motdepasse123
    IP du robot (fixe) : 192.168.4.1

  Bibliothèques requises : AUCUNE (WiFi.h et WiFiUdp.h sont dans le core ESP32).

  BROCHAGE (XIAO ESP32S3) :
    Joystick VCC -> 3V3
    Joystick GND -> GND
    Joystick VRx -> D2  (GPIO3)
    Joystick VRy -> D10 (GPIO9)
    Joystick SW  -> D6  (GPIO43)
    LED statut   -> D5  (GPIO6) — via résistance ~220-330Ω, cathode vers GND

  LED DE STATUT :
    fixe             = connectée au robot, le robot répond
    clignote lent    = WiFi OK mais le robot ne répond pas
    clignote rapide  = recherche du WiFi ROBOT_ESP32

  IDENTIFICATION USB : taper "?" dans le Moniteur Série -> répond "ID=MANETTE".
  Utilisé par tools/televerser.py pour trouver le bon port COM quand les deux
  ESP32 sont branchés en même temps.

  ⚠️ Ne pas toucher le joystick pendant la première seconde après le démarrage :
  sa position de repos est mesurée à ce moment-là (calibration automatique).
*/

#include <WiFi.h>
#include <WiFiUdp.h>

const char* IDENTITE = "MANETTE"; // renvoyé sur le port série quand on tape "?"

// =========================================================
// RÉSEAU DU ROBOT À REJOINDRE
// =========================================================
const char* ssidRobot = "ROBOT_ESP32";
const char* passwordRobot = "motdepasse123";
const IPAddress ipRobot(192, 168, 4, 1); // IP fixe du robot en mode point d'accès
const uint16_t PORT_UDP_ROBOT = 4210;
const uint16_t PORT_UDP_MANETTE = 4211;
WiFiUDP udp;

// =========================================================
// BROCHAGE
// =========================================================
const int PIN_JOYSTICK_X  = 3;  // VRx
const int PIN_JOYSTICK_Y  = 9;  // VRy
const int PIN_JOYSTICK_SW = 43; // bouton (appui = arrêt d'urgence)
const int PIN_LED_STATUT  = 6;

const int JOYSTICK_ZONE_MORTE = 300; // en dessous, on considère le stick au repos
const int PUISSANCE_MAX = 100;       // % de puissance à fond de stick
int centreX = 2048, centreY = 2048;  // recalculés au démarrage (calibration)

// =========================================================
// ÉTAT DE LA LIAISON ET INFOS REÇUES DU ROBOT
// =========================================================
const unsigned long INTERVALLE_ENVOI_MS = 40; // 25 fois par seconde
const unsigned long LIAISON_PERDUE_MS = 1000;
unsigned long derniereEnvoiMs = 0;
unsigned long derniereReponseOkMs = 0;
bool robotRepond = false;

struct InfosRobot {
  String pilote = "?";
  String gpsEtat = "?";
  bool gpsFix = false;
  double latitude = 0, longitude = 0;
  int satellites = 0;
} infosRobot;

// =========================================================
// LED DE STATUT
// =========================================================
void mettreAJourLed() {
  bool allumee;
  if (WiFi.status() != WL_CONNECTED) allumee = (millis() / 100) % 2;   // rapide : pas de WiFi
  else if (!robotRepond) allumee = (millis() / 500) % 2;               // lent : WiFi OK, robot muet
  else allumee = true;                                                 // fixe : tout communique
  digitalWrite(PIN_LED_STATUT, allumee ? HIGH : LOW);
}

// =========================================================
// JOYSTICK
// =========================================================
void calibrerJoystick() {
  long sommeX = 0, sommeY = 0;
  const int N = 32;
  for (int i = 0; i < N; i++) {
    sommeX += analogRead(PIN_JOYSTICK_X);
    sommeY += analogRead(PIN_JOYSTICK_Y);
    delay(5);
  }
  int cx = sommeX / N, cy = sommeY / N;
  // Garde-fou : si le stick était tenu pendant la calibration, on garde le centre théorique.
  if (cx > 1000 && cx < 3100) centreX = cx;
  if (cy > 1000 && cy < 3100) centreY = cy;
  Serial.printf("[Joystick] Centre calibré : X=%d  Y=%d\n", centreX, centreY);
}

// Ramène une lecture ADC brute dans -1..1 (0 = repos), chaque côté normalisé séparément
// car le centre réel n'est presque jamais exactement au milieu de 0-4095.
float normaliserAxe(int brut, int centre) {
  int d = brut - centre;
  float n = (d >= 0) ? (float)d / (float)(4095 - centre) : (float)d / (float)centre;
  return constrain(n, -1.0f, 1.0f);
}

// =========================================================
// ÉCHANGE AVEC LE ROBOT (UDP, non bloquant)
// =========================================================
String valeurChamp(const String& reponse, const char* cle) {
  String motif = String(";") + cle + "=";
  int debut = reponse.indexOf(motif);
  if (debut < 0) return "";
  debut += motif.length();
  int fin = reponse.indexOf(';', debut);
  return fin < 0 ? reponse.substring(debut) : reponse.substring(debut, fin);
}

bool analyserReponse(const String& r) {
  if (!r.startsWith("OK")) return false;
  infosRobot.pilote = valeurChamp(r, "pilote");
  infosRobot.gpsEtat = valeurChamp(r, "gps");
  infosRobot.gpsFix = valeurChamp(r, "fix") == "1";
  infosRobot.latitude = valeurChamp(r, "lat").toDouble();
  infosRobot.longitude = valeurChamp(r, "lon").toDouble();
  infosRobot.satellites = valeurChamp(r, "sat").toInt();
  return true;
}

void envoyerCommande(int pwrRoueG, int pwrRoueD, float jx, float jy, bool bouton) {
  char paquet[64];
  int n = snprintf(paquet, sizeof(paquet), "M,%d,%d,%.2f,%.2f,%d,%d",
                   pwrRoueG, pwrRoueD, jx, jy, bouton ? 1 : 0, (int)WiFi.RSSI());
  udp.beginPacket(ipRobot, PORT_UDP_ROBOT);
  udp.write((const uint8_t*)paquet, n);
  udp.endPacket();
}

// Lit les réponses du robot déjà arrivées (sans attendre)
void lireReponsesRobot() {
  int taille;
  while ((taille = udp.parsePacket()) > 0) {
    char tampon[160];
    int lu = udp.read((uint8_t*)tampon, sizeof(tampon) - 1);
    if (lu <= 0) continue;
    tampon[lu] = 0;
    if (analyserReponse(String(tampon))) {
      derniereReponseOkMs = millis();
      if (!robotRepond) Serial.println("[Robot] Liaison établie : commandes et GPS échangés.");
      robotRepond = true;
    }
  }
  if (robotRepond && millis() - derniereReponseOkMs > LIAISON_PERDUE_MS) {
    robotRepond = false;
    Serial.println("[Robot] Ne répond plus.");
  }
}

// =========================================================
// RECONNEXION AUTOMATIQUE AU ROBOT SI LA LIAISON TOMBE
// =========================================================
unsigned long dernierEssaiConnexionMs = 0;
const unsigned long INTERVALLE_RECONNEXION_MS = 5000;
bool etaitConnecte = false;

void gererConnexionWifi() {
  bool connecte = (WiFi.status() == WL_CONNECTED);
  if (connecte != etaitConnecte) {
    etaitConnecte = connecte;
    if (connecte) {
      Serial.print("[WiFi] Connectée au robot ! IP de la Manette : ");
      Serial.println(WiFi.localIP());
      udp.begin(PORT_UDP_MANETTE);
    } else {
      Serial.println("[WiFi] Connexion au robot perdue...");
      robotRepond = false;
    }
  }
  if (connecte) return;
  if (millis() - dernierEssaiConnexionMs > INTERVALLE_RECONNEXION_MS) {
    dernierEssaiConnexionMs = millis();
    Serial.println("[WiFi] Nouvelle tentative de connexion à ROBOT_ESP32 (le robot est-il allumé ?)");
    WiFi.disconnect();
    WiFi.begin(ssidRobot, passwordRobot);
  }
}

// =========================================================
// IDENTIFICATION SUR LE PORT USB
// =========================================================
void repondreIdentification() {
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c == '?') {
      Serial.print("ID=");
      Serial.println(IDENTITE);
    }
  }
}

// =========================================================
// AFFICHAGE MONITEUR SÉRIE (1 fois par seconde)
// =========================================================
void afficherEtat(int x, int y, bool bouton, int pwrG, int pwrD) {
  static unsigned long dernierAffichageMs = 0;
  if (millis() - dernierAffichageMs < 1000) return;
  dernierAffichageMs = millis();

  Serial.printf("[Joystick] X=%d Y=%d Bouton=%s -> rg=%d rd=%d | ", x, y, bouton ? "APPUYÉ" : "relâché", pwrG, pwrD);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Robot : WiFi non connecté");
    return;
  }
  if (!robotRepond) {
    Serial.println("Robot : ne répond pas");
    return;
  }
  Serial.printf("Robot : OK (pilote=%s, RSSI %d dBm) | GPS : ", infosRobot.pilote.c_str(), (int)WiFi.RSSI());
  if (infosRobot.gpsFix) {
    Serial.printf("%.6f, %.6f (%d satellites)%s\n", infosRobot.latitude, infosRobot.longitude, infosRobot.satellites,
                  infosRobot.gpsEtat == "simulation" ? " [SIMULATION]" : "");
  } else if (infosRobot.gpsEtat == "absent") {
    Serial.println("aucun module GPS trouvé sur le robot (câblage ?)");
  } else if (infosRobot.gpsEtat == "detection") {
    Serial.println("le robot cherche son module GPS (broche / vitesse)...");
  } else {
    Serial.printf("recherche des satellites... (état : %s)\n", infosRobot.gpsEtat.c_str());
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

  pinMode(PIN_JOYSTICK_SW, INPUT_PULLUP);
  pinMode(PIN_LED_STATUT, OUTPUT);
  calibrerJoystick();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // sans ça, le mode économie d'énergie ajoute jusqu'à ~100 ms de latence
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssidRobot, passwordRobot);
  dernierEssaiConnexionMs = millis();
  Serial.print("[WiFi] Connexion au robot (");
  Serial.print(ssidRobot);
  Serial.print(")");

  unsigned long debut = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - debut < 15000) {
    delay(300);
    Serial.print(".");
    mettreAJourLed();
    repondreIdentification();
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Robot injoignable pour l'instant — nouvelles tentatives en continu.");
  }
}

// =========================================================
// LOOP
// =========================================================
void loop() {
  gererConnexionWifi();
  mettreAJourLed();
  repondreIdentification();
  if (WiFi.status() == WL_CONNECTED) lireReponsesRobot();

  if (millis() - derniereEnvoiMs < INTERVALLE_ENVOI_MS) return;
  derniereEnvoiMs = millis();

  bool boutonAppuye = (digitalRead(PIN_JOYSTICK_SW) == LOW); // actif à l'état bas (pull-up interne)
  int x = analogRead(PIN_JOYSTICK_X);
  int y = analogRead(PIN_JOYSTICK_Y);
  float jx = normaliserAxe(x, centreX);
  float jy = normaliserAxe(y, centreY);

  int pwrRoueG = 0, pwrRoueD = 0;
  bool auRepos = abs(x - centreX) < JOYSTICK_ZONE_MORTE && abs(y - centreY) < JOYSTICK_ZONE_MORTE;
  if (!boutonAppuye && !auRepos) {
    float avance = -jy; // retirer le - si l'effet est inversé
    float tourne = jx;
    pwrRoueG = constrain((int)((avance + tourne) * PUISSANCE_MAX), -100, 100);
    pwrRoueD = constrain((int)((avance - tourne) * PUISSANCE_MAX), -100, 100);
  }
  // bouton appuyé : on envoie 0/0 ET bouton=1 -> le robot déclenche l'arrêt d'urgence
  // pour TOUTES les sources (IHM comprise).

  if (WiFi.status() == WL_CONNECTED) envoyerCommande(pwrRoueG, pwrRoueD, jx, jy, boutonAppuye);

  afficherEtat(x, y, boutonAppuye, pwrRoueG, pwrRoueD);
}
