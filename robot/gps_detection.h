/*
  Détection automatique du module GPS branché sur l'ESP32 Robot.

  Le robot ne sait pas forcément sur quelle broche arrive le fil TX du GPS, ni à
  quelle vitesse le module parle. Ce détecteur essaie, sans jamais bloquer la boucle :
    - chaque broche libre (D7 d'abord, puis D6, D2, D10),
    - chaque vitesse courante (9600 d'abord, puis 115200, 38400, 57600, 19200, 4800),
  et retient la première combinaison qui donne de VRAIES trames NMEA (au moins
  2 phrases "$....*hh" dont la somme de contrôle est correcte).

  - Une broche totalement muette à la première vitesse est abandonnée tout de suite
    (inutile d'essayer les autres vitesses) : un balayage complet dure quelques secondes.
  - Des octets reçus sans aucune trame valide = module présent mais mauvaise vitesse
    ou masse non commune -> signalé comme "illisible".
*/
#pragma once
#include <Arduino.h>

template <class SerieT>
class DetecteurGps {
 public:
  struct Broche { int gpio; const char* nom; };
  enum Etat : uint8_t { EN_COURS, TROUVE, INTROUVABLE };

  static const unsigned long DUREE_ESSAI_MS = 1500;  // un GPS envoie au moins une salve par seconde
  static const uint8_t TRAMES_POUR_VALIDER = 2;

  DetecteurGps(SerieT& serie, const Broche* broches, uint8_t nbBroches, const unsigned long* bauds, uint8_t nbBauds)
    : serie(serie), broches(broches), nbBroches(nbBroches), bauds(bauds), nbBauds(nbBauds) {}

  // Lance un balayage. Si une combinaison mémorisée est donnée, elle est essayée en premier.
  void demarrer(int gpioMemorise = -1, unsigned long baudsMemorises = 0) {
    etatActuel = EN_COURS;
    illisibleVu = false;
    essaiMemorise = false;
    for (uint8_t i = 0; i < nbBroches && baudsMemorises; i++) {
      for (uint8_t j = 0; j < nbBauds; j++) {
        if (broches[i].gpio == gpioMemorise && bauds[j] == baudsMemorises) { iBroche = i; iBauds = j; essaiMemorise = true; }
      }
    }
    if (!essaiMemorise) { iBroche = 0; iBauds = 0; }
    ouvrirEssai();
  }

  // À appeler pour chaque octet reçu du GPS (vérifie les sommes de contrôle NMEA).
  void caractereRecu(char c) {
    octetsEssai++;
    if (c == '$') { dansTrame = true; apresEtoile = 0; somme = 0; longueur = 0; return; }
    if (!dansTrame) return;
    if (++longueur > 90) { dansTrame = false; return; } // une phrase NMEA fait 82 caractères max
    if (apresEtoile == 0) {
      if (c == '*') apresEtoile = 1;
      else somme ^= (uint8_t)c;
    } else {
      int v = hex(c);
      if (v < 0) { dansTrame = false; return; }
      if (apresEtoile == 1) { sommeLue = v << 4; apresEtoile = 2; }
      else {
        dansTrame = false;
        if ((sommeLue | v) == somme) { tramesValidesEssai++; tramesValidesTotal++; }
      }
    }
  }

  // À appeler à chaque tour de loop() : passe à l'essai suivant quand il le faut.
  void mettreAJour() {
    if (etatActuel != EN_COURS) return;
    if (tramesValidesEssai >= TRAMES_POUR_VALIDER) {
      etatActuel = TROUVE;
      nouvelleDetection = true;
      return;
    }
    if (millis() - debutEssaiMs < DUREE_ESSAI_MS) return;

    bool ligneMuette = (octetsEssai == 0);
    if (octetsEssai > 20) illisibleVu = true; // des octets, mais pas de NMEA valide
    if (essaiMemorise) { essaiMemorise = false; iBroche = 0; iBauds = 0; }
    else if (ligneMuette || ++iBauds >= nbBauds) { iBauds = 0; iBroche++; }
    if (iBroche >= nbBroches) {
      etatActuel = INTROUVABLE;
      finMs = millis();
      return;
    }
    ouvrirEssai();
  }

  Etat etat() const { return etatActuel; }
  bool illisible() const { return etatActuel == INTROUVABLE && illisibleVu; } // octets reçus mais jamais de NMEA valide
  int gpio() const { return etatActuel == TROUVE ? broches[iBroche].gpio : -1; }
  const char* nomBroche() const { return etatActuel == TROUVE ? broches[iBroche].nom : ""; }
  unsigned long vitesse() const { return etatActuel == TROUVE ? bauds[iBauds] : 0; }
  unsigned long depuisFinMs() const { return millis() - finMs; }
  unsigned long tramesValides() const { return tramesValidesTotal; }

  // Vrai une seule fois juste après une détection réussie (pour afficher / mémoriser).
  bool vientDeTrouver() { bool v = nouvelleDetection; nouvelleDetection = false; return v; }

  // Description de l'essai en cours, ex. "D6 (GPIO43) à 115200 bauds"
  void decrireEssai(char* sortie, size_t taille) const {
    if (iBroche >= nbBroches) { snprintf(sortie, taille, "-"); return; }
    snprintf(sortie, taille, "%s (GPIO%d) à %lu bauds", broches[iBroche].nom, broches[iBroche].gpio, bauds[iBauds]);
  }

 private:
  SerieT& serie;
  const Broche* broches;
  uint8_t nbBroches;
  const unsigned long* bauds;
  uint8_t nbBauds;

  Etat etatActuel = INTROUVABLE;
  uint8_t iBroche = 0, iBauds = 0;
  bool essaiMemorise = false, illisibleVu = false, nouvelleDetection = false;
  unsigned long debutEssaiMs = 0, finMs = 0, octetsEssai = 0, tramesValidesTotal = 0;
  uint8_t tramesValidesEssai = 0;

  // Analyse NMEA
  bool dansTrame = false;
  uint8_t apresEtoile = 0, somme = 0, sommeLue = 0, longueur = 0;

  static int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  }

  void ouvrirEssai() {
    serie.end();
    serie.setRxBufferSize(1024);
    serie.begin(bauds[iBauds], SERIAL_8N1, broches[iBroche].gpio, -1);
    debutEssaiMs = millis();
    octetsEssai = 0;
    tramesValidesEssai = 0;
    dansTrame = false;
  }
};
