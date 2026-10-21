/*
  Liste des satellites vus par le GPS, avec la force de leur signal.

  Décode les trames NMEA "GSV" (GPGSV, BDGSV, GLGSV, GAGSV...) : pour chaque satellite,
  son numéro, sa hauteur dans le ciel, sa direction et la force du signal reçu
  (SNR en dB-Hz). C'est LE bon indicateur de la qualité de réception :
    - 35 dB-Hz et plus : très bon signal
    - 25 à 35 dB-Hz    : signal moyen
    - moins de 25      : trop faible pour être utilisé (toit, murs, corps, antenne retournée...)
  Il faut au moins 4 satellites avec un bon signal pour calculer une position, et
  6 ou plus pour qu'elle soit précise.
*/
#pragma once
#include <Arduino.h>

class SatellitesGps {
 public:
  struct Satellite {
    char systeme;          // 'G' GPS, 'B' BeiDou, 'R' GLONASS, 'E' Galileo, 'Q' QZSS, '?' inconnu
    uint8_t numero;
    int8_t elevation;      // degrés au-dessus de l'horizon (-1 = inconnue)
    int16_t azimut;        // degrés depuis le nord (-1 = inconnu)
    int8_t snr;            // dB-Hz (-1 = satellite prévu mais pas encore reçu)
    unsigned long vuMs;
  };
  static const int MAX_SATELLITES = 40;
  static const unsigned long DUREE_VALIDITE_MS = 5000;

  // À appeler pour chaque caractère reçu du GPS.
  void caractere(char c) {
    if (c == '$') { n = 0; enCours = true; return; }
    if (!enCours) return;
    if (c == '\r' || c == '\n') { enCours = false; if (n > 0) traiterPhrase(); return; }
    if (n < sizeof(tampon) - 1) tampon[n++] = c;
    else enCours = false; // trop long : pas une trame NMEA
  }

  // Satellites vus depuis moins de 5 s
  int nombre() const {
    int k = 0;
    for (int i = 0; i < nb; i++) if (frais(sats[i])) k++;
    return k;
  }
  int nombreAvecSignal(int snrMin) const {
    int k = 0;
    for (int i = 0; i < nb; i++) if (frais(sats[i]) && sats[i].snr >= snrMin) k++;
    return k;
  }
  // Parcours : for (int i = 0; i < s.taille(); i++) if (s.estFrais(i)) { const auto& x = s.sat(i); ... }
  int taille() const { return nb; }
  bool estFrais(int i) const { return frais(sats[i]); }
  const Satellite& sat(int i) const { return sats[i]; }

 private:
  Satellite sats[MAX_SATELLITES];
  int nb = 0;
  char tampon[100];
  size_t n = 0;
  bool enCours = false;

  static bool frais(const Satellite& s) { return millis() - s.vuMs < DUREE_VALIDITE_MS; }

  static char systemeDepuis(const char* talker) {
    if (talker[0] == 'G' && talker[1] == 'P') return 'G';
    if ((talker[0] == 'B' && talker[1] == 'D') || (talker[0] == 'G' && talker[1] == 'B')) return 'B';
    if (talker[0] == 'G' && talker[1] == 'L') return 'R';
    if (talker[0] == 'G' && talker[1] == 'A') return 'E';
    if (talker[0] == 'G' && talker[1] == 'Q') return 'Q';
    return '?';
  }

  void traiterPhrase() {
    tampon[n] = 0;
    char* etoile = strchr(tampon, '*');
    if (!etoile) return;
    uint8_t somme = 0;
    for (char* p = tampon; p < etoile; p++) somme ^= (uint8_t)*p;
    if ((uint8_t)strtol(etoile + 1, nullptr, 16) != somme) return; // trame abîmée
    *etoile = 0;
    if (strlen(tampon) < 6 || strncmp(tampon + 2, "GSV", 3) != 0) return;

    char systeme = systemeDepuis(tampon);
    // Découpe en champs (les champs vides sont conservés : ",," = SNR inconnu)
    char* champs[24];
    int nc = 0;
    champs[nc++] = tampon;
    for (char* p = tampon; *p && nc < 24; p++) {
      if (*p == ',') { *p = 0; champs[nc++] = p + 1; }
    }
    // champs : 0 = $xxGSV, 1 = nb de trames, 2 = n° de trame, 3 = satellites en vue,
    // puis des groupes de 4 : numéro, élévation, azimut, SNR (+ un "signal ID" en NMEA 4.1)
    for (int i = 4; i + 3 < nc; i += 4) {
      if (!*champs[i]) continue;
      mettreAJour(systeme, (uint8_t)atoi(champs[i]),
                  *champs[i + 1] ? (int8_t)atoi(champs[i + 1]) : (int8_t)-1,
                  *champs[i + 2] ? (int16_t)atoi(champs[i + 2]) : (int16_t)-1,
                  *champs[i + 3] ? (int8_t)atoi(champs[i + 3]) : (int8_t)-1);
    }
  }

  void mettreAJour(char systeme, uint8_t numero, int8_t elevation, int16_t azimut, int8_t snr) {
    int cible = -1, plusVieux = 0;
    for (int i = 0; i < nb; i++) {
      if (sats[i].systeme == systeme && sats[i].numero == numero) { cible = i; break; }
      if (sats[i].vuMs < sats[plusVieux].vuMs) plusVieux = i;
    }
    if (cible < 0) cible = nb < MAX_SATELLITES ? nb++ : plusVieux; // liste pleine : on remplace le plus ancien
    sats[cible] = { systeme, numero, elevation, azimut, snr, millis() };
  }
};
