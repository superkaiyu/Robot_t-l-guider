/*
  Journal des sessions : la "boîte noire" du robot.

  Une session = de l'allumage du robot à son extinction. Chaque session est un
  fichier CSV dans la mémoire flash (système de fichiers LittleFS) :

      /sessions/000042.csv      (42 = numéro de démarrage du robot)

  Contenu :
      #session,42,mise sous tension,ESP32-S3 N16R8          <- en-tête
      t,lat,lon,fix,sats,hdop,vitesse_kmh,...              <- noms des colonnes
      12,45.648021,0.137802,1,8,0.9,0.0,11.84,0.21,...     <- une mesure (t en s)
      #ev,15,urgence,Arrêt d'urgence                       <- un événement
      #horloge,1793612345,20                               <- heure réelle (s depuis 1970) à t = 20 s

  Pour ménager la flash, les lignes sont gardées en mémoire et écrites toutes les
  10 s. Quand la place manque, les sessions les plus anciennes sont effacées.

  Arduino IDE : il faut un schéma de partition avec un espace de fichiers,
  par exemple "Huge APP (3MB No OTA/1MB SPIFFS)". Sans lui, le robot fonctionne
  normalement mais n'enregistre rien (l'IHM le signale).
*/
#pragma once
#include <Arduino.h>
#include <LittleFS.h>

class JournalSession {
 public:
  static const size_t ESPACE_A_GARDER = 64 * 1024; // toujours au moins 64 Ko libres
  static const int SESSIONS_MAX = 60;
  static const unsigned long INTERVALLE_ECRITURE_MS = 10000;

  // À appeler une fois au démarrage. Renvoie false si la flash n'a pas d'espace de fichiers.
  bool demarrer(uint32_t numero, const char* cause, const char* carte) {
    actif = LittleFS.begin(true); // true : formate l'espace au tout premier démarrage
    if (!actif) return false;
    LittleFS.mkdir("/sessions");
    numeroSession = numero;
    cheminSession(numero, chemin, sizeof(chemin));
    nettoyer();
    tampon.reserve(2048);
    tampon = "#session,";
    tampon += String(numero) + "," + propre(cause) + "," + propre(carte) + "\n";
    tampon += "t,lat,lon,fix,sats,hdop,vitesse_kmh,batterie_v,courant_a,conso_ah,pwr_g,pwr_d,pilote,ihm,manette,rssi_min\n";
    ecrire();
    return true;
  }

  bool disponible() const { return actif; }
  bool plein() const { return memoirePleine; }
  uint32_t numero() const { return numeroSession; }

  void mesure(const String& ligne) {
    if (!actif || memoirePleine) return;
    tampon += ligne;
    tampon += '\n';
    if (tampon.length() > 1800) ecrire();
  }

  // Événement horodaté (t = secondes depuis l'allumage). Les virgules du texte sont remplacées.
  void evenement(uint32_t t, const char* code, const String& texte) {
    if (!actif || memoirePleine) return;
    tampon += "#ev," + String(t) + "," + code + "," + propre(texte) + "\n";
  }

  // Heure réelle connue (envoyée par l'IHM ou lue sur le GPS)
  void horloge(uint32_t epoch, uint32_t t) {
    if (!actif || horlogeNotee) return;
    horlogeNotee = true;
    tampon += "#horloge," + String(epoch) + "," + String(t) + "\n";
    File index = LittleFS.open("/sessions/index.csv", "a"); // numéro -> heure de début, pour la liste
    if (index) {
      index.printf("%lu,%lu\n", (unsigned long)numeroSession, (unsigned long)(epoch - t));
      index.close();
    }
  }

  // À appeler à chaque tour de loop() : écrit le tampon toutes les 10 s.
  void loop() {
    if (actif && millis() - dernierEcritureMs > INTERVALLE_ECRITURE_MS) ecrire();
  }

  // Écrit tout de suite ce qui est en attente (avant un téléchargement par exemple).
  void ecrire() {
    dernierEcritureMs = millis();
    if (!actif || tampon.length() == 0) return;
    if (espaceLibre() < ESPACE_A_GARDER) {
      libererPlace();
      if (espaceLibre() < ESPACE_A_GARDER / 2) { memoirePleine = true; tampon = ""; return; }
    }
    File f = LittleFS.open(chemin, "a");
    if (!f) return;
    f.print(tampon);
    f.close();
    tampon = "";
  }

  size_t espaceLibre() const { return actif ? LittleFS.totalBytes() - LittleFS.usedBytes() : 0; }
  size_t espaceTotal() const { return actif ? LittleFS.totalBytes() : 0; }

  // {"disponible":true,"libre":..,"total":..,"enCours":42,"sessions":[{"id":42,"taille":..,"duree":..,"debut":..,"cause":".."},...]}
  void listeJson(String& t, uint32_t dureeSessionEnCours) {
    t = "{\"disponible\":";
    t += actif ? "true" : "false";
    t += ",\"plein\":";
    t += memoirePleine ? "true" : "false";
    t += ",\"libre\":" + String(espaceLibre()) + ",\"total\":" + String(espaceTotal()) + ",\"enCours\":" + String(numeroSession) + ",\"sessions\":[";
    if (actif) {
      uint32_t numeros[SESSIONS_MAX + 8];
      int n = listerNumeros(numeros, SESSIONS_MAX + 8);
      for (int i = n - 1; i >= 0; i--) { // la plus récente d'abord
        char p[32];
        cheminSession(numeros[i], p, sizeof(p));
        File f = LittleFS.open(p, "r");
        if (!f) continue;
        size_t taille = f.size();
        String cause = lireCause(f);
        uint32_t duree = numeros[i] == numeroSession ? dureeSessionEnCours : lireDerniereDate(f);
        f.close();
        if (numeros[i] == numeroSession) taille += tampon.length();
        if (t[t.length() - 1] != '[') t += ",";
        t += "{\"id\":" + String(numeros[i]) + ",\"taille\":" + String(taille) + ",\"duree\":" + String(duree) +
             ",\"debut\":" + String(debutSession(numeros[i])) + ",\"cause\":\"" + cause + "\"}";
      }
    }
    t += "]}";
  }

  bool ouvrir(uint32_t numero, File& f) {
    if (!actif) return false;
    if (numero == numeroSession) ecrire();
    char p[32];
    cheminSession(numero, p, sizeof(p));
    f = LittleFS.open(p, "r");
    return (bool)f;
  }

  bool supprimer(uint32_t numero) {
    if (!actif || numero == numeroSession) return false; // la session en cours ne s'efface pas
    char p[32];
    cheminSession(numero, p, sizeof(p));
    return LittleFS.remove(p);
  }

 private:
  bool actif = false, memoirePleine = false, horlogeNotee = false;
  uint32_t numeroSession = 0;
  char chemin[32] = "";
  String tampon;
  unsigned long dernierEcritureMs = 0;

  static void cheminSession(uint32_t numero, char* sortie, size_t taille) {
    snprintf(sortie, taille, "/sessions/%06lu.csv", (unsigned long)numero);
  }

  static String propre(const String& texte) { // pas de virgule ni de retour à la ligne dans un champ CSV
    String r = texte;
    r.replace(",", ";");
    r.replace("\n", " ");
    r.replace("\"", "'");
    return r;
  }

  // Numéros des sessions présentes, triés du plus ancien au plus récent
  int listerNumeros(uint32_t* numeros, int max) {
    int n = 0;
    File dossier = LittleFS.open("/sessions");
    if (!dossier) return 0;
    for (File f = dossier.openNextFile(); f && n < max; f = dossier.openNextFile()) {
      String nom = f.name();
      int barre = nom.lastIndexOf('/');
      if (barre >= 0) nom = nom.substring(barre + 1); // selon la version du core, name() contient ou non le dossier
      if (nom.endsWith(".csv") && nom != "index.csv") numeros[n++] = (uint32_t)nom.toInt();
    }
    for (int i = 1; i < n; i++) { // tri par insertion
      uint32_t v = numeros[i];
      int j = i - 1;
      while (j >= 0 && numeros[j] > v) { numeros[j + 1] = numeros[j]; j--; }
      numeros[j + 1] = v;
    }
    return n;
  }

  bool supprimerPlusAncienne() {
    uint32_t numeros[SESSIONS_MAX + 8];
    int n = listerNumeros(numeros, SESSIONS_MAX + 8);
    for (int i = 0; i < n; i++) {
      if (numeros[i] != numeroSession) return supprimer(numeros[i]);
    }
    return false;
  }

  void libererPlace() {
    while (espaceLibre() < ESPACE_A_GARDER && supprimerPlusAncienne()) {}
  }

  // Au démarrage : efface les sessions vides (redémarrages pendant un téléversement...)
  // et les plus anciennes au-delà de SESSIONS_MAX.
  void nettoyer() {
    uint32_t numeros[SESSIONS_MAX + 8];
    int n = listerNumeros(numeros, SESSIONS_MAX + 8);
    int restantes = n;
    for (int i = 0; i < n; i++) {
      char p[32];
      cheminSession(numeros[i], p, sizeof(p));
      File f = LittleFS.open(p, "r");
      size_t taille = f ? f.size() : 0;
      if (f) f.close();
      if (taille < 400 || restantes > SESSIONS_MAX) { LittleFS.remove(p); restantes--; }
    }
    libererPlace();
  }

  static String lireCause(File& f) { // 3e champ de la 1re ligne "#session,42,cause,carte"
    f.seek(0);
    String l = f.readStringUntil('\n');
    int a = l.indexOf(','), b = l.indexOf(',', a + 1), c = l.indexOf(',', b + 1);
    return (a >= 0 && b > a && c > b) ? l.substring(b + 1, c) : String("");
  }

  static uint32_t lireDerniereDate(File& f) { // t de la dernière mesure du fichier
    size_t taille = f.size();
    f.seek(taille > 300 ? taille - 300 : 0);
    String fin = f.readString();
    uint32_t t = 0;
    int debut = 0;
    while (debut < (int)fin.length()) {
      int finLigne = fin.indexOf('\n', debut);
      if (finLigne < 0) finLigne = fin.length();
      if (fin[debut] >= '0' && fin[debut] <= '9') t = (uint32_t)fin.substring(debut, fin.indexOf(',', debut)).toInt();
      debut = finLigne + 1;
    }
    return t;
  }

  uint32_t debutSession(uint32_t numero) { // heure de début (s depuis 1970), 0 si inconnue
    File index = LittleFS.open("/sessions/index.csv", "r");
    if (!index) return 0;
    uint32_t trouve = 0;
    while (index.available()) {
      String l = index.readStringUntil('\n');
      int v = l.indexOf(',');
      if (v > 0 && (uint32_t)l.substring(0, v).toInt() == numero) trouve = (uint32_t)l.substring(v + 1).toInt();
    }
    index.close();
    return trouve;
  }
};
