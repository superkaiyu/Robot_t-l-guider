/*
  Filtre de position GPS : empêche le tracé de "bouger tout seul".

  Même posé sur une table, un GPS ne donne jamais deux fois la même position :
  d'une seconde à l'autre elle varie de 2 à 5 m dehors, et de 10 à 20 m en intérieur
  ou près d'une fenêtre. Ce filtre :
    - IGNORE les mesures trop imprécises (HDOP > 8, moins de 4 satellites) ;
    - quand le robot est IMMOBILE (moteurs arrêtés) : MOYENNE les mesures pendant 30 s,
      puis FIGE la position jusqu'au prochain mouvement. La dérive lente du GPS (en intérieur, elle peut
      atteindre plusieurs dizaines de mètres en quelques minutes) et les sauts
      (reflets) ne déplacent PAS la position ;
    - quand le robot BOUGE (moteurs actifs, ou vraiment porté à la main : vitesse de
      marche > 3,5 km/h pendant 5 s ET au moins 4 m parcourus) : suit les mesures avec
      un léger lissage, et ignore les sauts aberrants isolés (> 50 m d'un coup).

  Pourquoi ne pas se fier à la vitesse du GPS seule : même immobile, un GPS annonce
  souvent 1 à 3 km/h de "vitesse" (bruit), ce qui faisait croire à un déplacement.
*/
#pragma once
#include <math.h>

class FiltreGps {
 public:
  static constexpr float VITESSE_MARCHE_KMH = 3.5f; // porté à la main : au moins une marche lente
  static constexpr float VITESSE_ARRET_KMH = 2.0f;
  static constexpr int MESURES_MARCHE = 5;          // vitesse de marche pendant 5 mesures (5 s)...
  static constexpr float DISTANCE_MARCHE_M = 4.0f;  // ...et au moins 4 m réellement parcourus
  static constexpr float HDOP_MAX = 8.0f;
  static constexpr int SATELLITES_MIN = 4;
  static constexpr float SAUT_MAX_M = 50.0f;
  static constexpr int MESURES_AVANT_FIGEAGE = 30; // immobile : 30 s de moyenne, puis position figée

  // À appeler à chaque NOUVELLE mesure du GPS (en général 1 fois par seconde).
  void ajouter(double lat, double lon, float hdop, int satellites, float vitesseKmh, bool moteursActifs) {
    latBrute = lat; lonBrute = lon; hdopActuel = hdop;
    if (hdop > HDOP_MAX || satellites < SATELLITES_MIN) { rejetees++; return; } // mesure trop imprécise
    if (!initialise) { repartirDe(lat, lon); initialise = true; return; }

    float ecart = distanceM(lat, lon, latF, lonF);

    // Porté à la main ? Vitesse de marche soutenue ET mesures qui avancent vraiment.
    mesuresRapides = vitesseKmh > VITESSE_MARCHE_KMH ? mesuresRapides + 1 : 0;
    mesuresLentes = vitesseKmh < VITESSE_ARRET_KMH ? mesuresLentes + 1 : 0;
    bool historiquePlein = nHistorique == MESURES_MARCHE;
    float parcouru = historiquePlein ? distanceM(lat, lon, histLat[iHistorique], histLon[iHistorique]) : 0;
    histLat[iHistorique] = lat; histLon[iHistorique] = lon;
    iHistorique = (iHistorique + 1) % MESURES_MARCHE;
    if (nHistorique < MESURES_MARCHE) nHistorique++;
    if (!porteALaMain && mesuresRapides >= MESURES_MARCHE && parcouru > DISTANCE_MARCHE_M) porteALaMain = true;
    if (porteALaMain && mesuresLentes >= 3) porteALaMain = false;
    bool mouvement = moteursActifs || porteALaMain;

    if (ecart > SAUT_MAX_M && vitesseKmh < 20) {
      if (!mouvement) { rejetees++; return; }             // immobile : un saut n'est que du bruit
      if (++sautsConsecutifs < 5) { rejetees++; return; } // reflet ponctuel : ignoré
      repartirDe(lat, lon);                               // le saut persiste en roulant : on suit
      return;
    }
    sautsConsecutifs = 0;

    if (mouvement) {
      immobileFlag = false;
      latF += 0.5 * (lat - latF);  // léger lissage
      lonF += 0.5 * (lon - lonF);
      return;
    }

    // Immobile : moyenne des 30 premières mesures, puis position FIGÉE jusqu'au prochain
    // mouvement (la dérive lente du GPS ne peut plus faire "glisser" le robot).
    if (!immobileFlag) { immobileFlag = true; nMoyenne = 1; }
    if (nMoyenne >= MESURES_AVANT_FIGEAGE) return;
    nMoyenne++;
    latF += (lat - latF) / nMoyenne;
    lonF += (lon - lonF) / nMoyenne;
  }

  bool pret() const { return initialise; }
  double lat() const { return latF; }
  double lon() const { return lonF; }
  double latitudeBrute() const { return latBrute; }
  double longitudeBrute() const { return lonBrute; }
  bool immobile() const { return immobileFlag; }
  int mesuresMoyennees() const { return immobileFlag ? nMoyenne : 0; }
  bool fige() const { return immobileFlag && nMoyenne >= MESURES_AVANT_FIGEAGE; }
  unsigned long mesuresRejetees() const { return rejetees; }
  // Précision typique d'une mesure seule : HDOP x ~2,5 m
  float precisionM() const { return hdopActuel * 2.5f; }

  static float distanceM(double lat1, double lon1, double lat2, double lon2) {
    double dy = (lat1 - lat2) * 110540.0;
    double dx = (lon1 - lon2) * 111320.0 * cos(lat2 * M_PI / 180.0);
    return (float)sqrt(dx * dx + dy * dy);
  }

 private:
  bool initialise = false, immobileFlag = false, porteALaMain = false;
  double latF = 0, lonF = 0, latBrute = 0, lonBrute = 0;
  float hdopActuel = 99;
  int nMoyenne = 0, sautsConsecutifs = 0, mesuresRapides = 0, mesuresLentes = 0;
  double histLat[MESURES_MARCHE] = {}, histLon[MESURES_MARCHE] = {}; // 5 dernières mesures
  int iHistorique = 0, nHistorique = 0;
  unsigned long rejetees = 0;

  void repartirDe(double lat, double lon) {
    latF = lat; lonF = lon;
    nMoyenne = 1; sautsConsecutifs = 0;
  }
};
