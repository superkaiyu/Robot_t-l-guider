/*
  Filtre de position GPS : empêche le tracé de "bouger tout seul".

  Même posé sur une table, un GPS ne donne jamais deux fois la même position :
  d'une seconde à l'autre elle varie de 2 à 5 m dehors, et de 10 à 20 m en intérieur
  ou près d'une fenêtre. Ce filtre :
    - IGNORE les mesures trop imprécises (HDOP > 8, moins de 4 satellites) ;
    - IGNORE les sauts aberrants (> 50 m d'un coup sans vitesse, souvent des reflets),
      sauf s'ils se répètent (le robot a vraiment été déplacé) ;
    - quand le robot est IMMOBILE (moteurs arrêtés ET vitesse GPS faible) : fige la
      position et la MOYENNE sur les mesures successives -> plus de dérive, et la
      position devient même plus précise avec le temps ;
    - quand le robot BOUGE (moteurs actifs, ou porté à la main : vitesse GPS > 2 km/h) :
      suit les mesures avec un léger lissage.
*/
#pragma once
#include <math.h>

class FiltreGps {
 public:
  static constexpr float VITESSE_MOUVEMENT_KMH = 2.0f;
  static constexpr float HDOP_MAX = 8.0f;
  static constexpr int SATELLITES_MIN = 4;
  static constexpr float SAUT_MAX_M = 50.0f;
  static constexpr int MESURES_MOYENNE_MAX = 600; // ~10 min de moyenne au maximum

  // À appeler à chaque NOUVELLE mesure du GPS (en général 1 fois par seconde).
  void ajouter(double lat, double lon, float hdop, int satellites, float vitesseKmh, bool moteursActifs) {
    latBrute = lat; lonBrute = lon; hdopActuel = hdop;
    if (hdop > HDOP_MAX || satellites < SATELLITES_MIN) { rejetees++; return; } // mesure trop imprécise
    if (!initialise) { repartirDe(lat, lon); initialise = true; return; }

    float ecart = distanceM(lat, lon, latF, lonF);
    if (ecart > SAUT_MAX_M && vitesseKmh < 20) {
      if (++sautsConsecutifs < 5) { rejetees++; return; } // reflet ponctuel : ignoré
      repartirDe(lat, lon);                               // le saut persiste : robot déplacé
      return;
    }
    sautsConsecutifs = 0;

    // Mouvement : moteurs actifs, ou vitesse GPS élevée sur 2 mesures de suite (robot porté).
    mesuresRapides = vitesseKmh > VITESSE_MOUVEMENT_KMH ? mesuresRapides + 1 : 0;
    bool mouvement = moteursActifs || mesuresRapides >= 2;

    if (mouvement) {
      immobileFlag = false;
      latF += 0.5 * (lat - latF);  // léger lissage
      lonF += 0.5 * (lon - lonF);
      return;
    }

    if (!immobileFlag) { immobileFlag = true; nMoyenne = 1; horsZone = 0; } // on part de la position actuelle
    // Déplacement lent sans moteurs (poussé à la main) : si les mesures restent loin
    // de la position moyennée pendant 5 s, on repart de la nouvelle position.
    float rayon = fmaxf(8.0f, 3.0f * precisionM());
    if (ecart > rayon) {
      if (++horsZone >= 5) repartirDe(lat, lon);
      return;
    }
    horsZone = 0;
    if (nMoyenne < MESURES_MOYENNE_MAX) nMoyenne++;
    latF += (lat - latF) / nMoyenne; // moyenne glissante
    lonF += (lon - lonF) / nMoyenne;
  }

  bool pret() const { return initialise; }
  double lat() const { return latF; }
  double lon() const { return lonF; }
  double latitudeBrute() const { return latBrute; }
  double longitudeBrute() const { return lonBrute; }
  bool immobile() const { return immobileFlag; }
  int mesuresMoyennees() const { return immobileFlag ? nMoyenne : 0; }
  unsigned long mesuresRejetees() const { return rejetees; }
  // Précision typique d'une mesure seule : HDOP x ~2,5 m
  float precisionM() const { return hdopActuel * 2.5f; }

  static float distanceM(double lat1, double lon1, double lat2, double lon2) {
    double dy = (lat1 - lat2) * 110540.0;
    double dx = (lon1 - lon2) * 111320.0 * cos(lat2 * M_PI / 180.0);
    return (float)sqrt(dx * dx + dy * dy);
  }

 private:
  bool initialise = false, immobileFlag = false;
  double latF = 0, lonF = 0, latBrute = 0, lonBrute = 0;
  float hdopActuel = 99;
  int nMoyenne = 0, sautsConsecutifs = 0, mesuresRapides = 0, horsZone = 0;
  unsigned long rejetees = 0;

  void repartirDe(double lat, double lon) {
    latF = lat; lonF = lon;
    nMoyenne = 1; horsZone = 0; sautsConsecutifs = 0;
  }
};
