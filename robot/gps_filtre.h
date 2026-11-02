/*
  Filtre de position GPS : empêche le tracé de "bouger tout seul".

  Même posé sur une table, un GPS ne donne jamais deux fois la même position.
  Dehors avec un bon signal, elle varie de 2 à 5 m. Avec un signal faible
  (intérieur, près d'une fenêtre, module sous un objet...), les mesures peuvent
  partir à 50 ou 150 m, même avec un HDOP qui a l'air bon. Le filtre :

    1. DÉMARRAGE : les premières positions après l'allumage sont souvent fausses.
       On attend que 10 mesures de suite soient d'accord entre elles (ou 60 s au plus)
       et on garde leur médiane (une valeur fausse n'a aucun poids).
    2. SIGNAL : on compte les satellites reçus à 25 dB-Hz ou plus. En dessous de 4,
       le signal est "faible" : la précision annoncée devient au moins 25 m et la
       position reste figée (pas de moyenne : elle dériverait) tant que les moteurs
       ne tournent pas.
    3. SAUTS IMPOSSIBLES : ni le robot ni quelqu'un qui le porte ne va à plus de
       3 m/s. Une mesure plus loin que (incertitude + 3 m/s x temps écoulé) est
       ignorée. Si 10 mesures de suite sont ignorées et qu'elles sont d'accord
       entre elles, c'est que le robot a vraiment été déplacé : on repart de là
       (compteur sauts(), l'IHM coupe alors le trait au lieu de tracer une ligne).
    4. IMMOBILE (moteurs arrêtés, pas porté) : moyenne pondérée pendant 60 s, puis
       position FIGÉE jusqu'au prochain mouvement.
    5. PORTÉ À LA MAIN (moteurs arrêtés mais signal bon) : seulement si la médiane des
       5 dernières mesures s'est éloignée de plus de 8 m (ou 2 x l'incertitude) ET
       que le GPS mesure une vitesse. La vitesse seule ne suffit pas : immobile, un GPS
       annonce souvent 1 à 3 km/h de bruit.
*/
#pragma once
#include <math.h>

class FiltreGps {
 public:
  static constexpr int MESURES_DEMARRAGE = 10;
  static constexpr float HDOP_MAX = 8.0f;
  static constexpr int SATELLITES_MIN = 4;
  static constexpr int SATS_FORTS_MIN = 4;          // satellites reçus à 25 dB-Hz ou plus
  static constexpr float VITESSE_MAX_MS = 3.0f;     // plus rapide qu'un marcheur pressé : impossible
  static constexpr int REJETS_AVANT_DEPLACEMENT = 10;
  static constexpr int MESURES_AVANT_FIGEAGE = 60;  // immobile : 60 s de moyenne, puis position figée
  static constexpr float DISTANCE_PORTE_M = 8.0f;
  static constexpr int N = 5;                       // mesures récentes gardées

  // À appeler à chaque NOUVELLE mesure du GPS (en général 1 fois par seconde).
  // satsForts : satellites reçus à 25 dB-Hz ou plus (-1 si inconnu).
  void ajouter(double lat, double lon, float hdop, int satellites, int satsForts, float vitesseKmh,
               bool moteursActifs, unsigned long maintenantMs) {
    latBrute = lat; lonBrute = lon; hdopActuel = hdop;
    signalFaibleFlag = satsForts >= 0 && satsForts < SATS_FORTS_MIN;
    if (hdop > HDOP_MAX || satellites < SATELLITES_MIN) { rejetees++; return; } // mesure trop imprécise

    // 1. Démarrage : 10 mesures de suite d'accord entre elles (60 s au plus)
    if (!initialise) {
      if (nDem == 0) debutDemarrageMs = maintenantMs;
      demLat[iDem] = lat; demLon[iDem] = lon;
      iDem = (iDem + 1) % MESURES_DEMARRAGE;
      if (nDem < MESURES_DEMARRAGE) nDem++;
      if (nDem < MESURES_DEMARRAGE) return;
      double mLat = mediane(demLat, nDem), mLon = mediane(demLon, nDem);
      float etendue = 0;
      for (int i = 0; i < nDem; i++) etendue = fmaxf(etendue, distanceM(demLat[i], demLon[i], mLat, mLon));
      if (etendue > fmaxf(15.0f, 3 * precisionM()) && maintenantMs - debutDemarrageMs < 60000UL) return;
      repartirDe(mLat, mLon, maintenantMs);
      initialise = true;
      return;
    }

    float dt = (maintenantMs - dernierAccepteMs) / 1000.0f;
    if (dt < 1) dt = 1;
    if (dt > 120) dt = 120;

    // Mesures récentes (même celles qu'on ignore : elles servent à voir un vrai déplacement)
    recLat[iRec] = lat; recLon[iRec] = lon;
    iRec = (iRec + 1) % N;
    if (nRec < N) nRec++;

    // 5. Porté à la main ?
    if (nRec == N) {
      double mLat = mediane(recLat, N), mLon = mediane(recLon, N);
      float eloignement = distanceM(mLat, mLon, latF, lonF);
      if (!porteALaMain && !signalFaibleFlag && vitesseKmh > 1.5f && eloignement > fmaxf(DISTANCE_PORTE_M, 2 * precisionM())) {
        porteALaMain = true;
      }
      if (porteALaMain && etendueRecente() < 3.0f && vitesseKmh < 2.0f) porteALaMain = false; // reposé
    }
    bool mouvement = moteursActifs || porteALaMain;

    // 3. Saut impossible ?
    float ecart = distanceM(lat, lon, latF, lonF);
    float limite = 2 * fmaxf(precisionM(), precisionAncre) + (mouvement ? VITESSE_MAX_MS * dt : 0);
    if (ecart > limite) {
      rejetees++;
      rejetsConsecutifs++;
      // Vraiment déplacé ? (10 mesures ignorées de suite, d'accord entre elles). Avec un
      // signal faible et des moteurs arrêtés, on ne croit pas à un déplacement : on reste figé.
      bool deplacementCredible = mouvement || !signalFaibleFlag;
      if (deplacementCredible && rejetsConsecutifs >= REJETS_AVANT_DEPLACEMENT && nRec == N
          && etendueRecente() < fmaxf(15.0f, 3 * precisionM())) {
        repartirDe(mediane(recLat, N), mediane(recLon, N), maintenantMs);
        nSauts++;
      }
      return;
    }
    rejetsConsecutifs = 0;
    dernierAccepteMs = maintenantMs;

    if (mouvement) {
      immobileFlag = false;
      latF += 0.5 * (lat - latF);  // léger lissage
      lonF += 0.5 * (lon - lonF);
      precisionAncre = precisionM();
      return;
    }

    // 4. Immobile : moyenne pondérée, puis position figée.
    // Si la position a été calculée avec un signal faible et que le signal redevient bon,
    // on recommence la moyenne avec les nouvelles mesures (bien meilleures).
    float w = poids(hdop, satellites);
    if (!immobileFlag || (ancreFaible && !signalFaibleFlag)) {
      immobileFlag = true;
      nMoyenne = 1;
      sommePoids = ancreFaible && !signalFaibleFlag ? w * 0.1 : w;
      ancreFaible = signalFaibleFlag;
      precisionAncre = precisionM();
    }
    if (nMoyenne >= MESURES_AVANT_FIGEAGE || signalFaibleFlag) return;
    // Moyenne pondérée : une mesure à HDOP 1 compte 4 fois plus qu'une mesure à HDOP 2.
    nMoyenne++;
    sommePoids += w;
    latF += (lat - latF) * (w / sommePoids);
    lonF += (lon - lonF) * (w / sommePoids);
    if (!signalFaibleFlag && precisionAncre > precisionM()) precisionAncre = precisionM();
  }

  bool pret() const { return initialise; }
  bool enDemarrage() const { return !initialise && nDem > 0; }
  int mesuresDemarrage() const { return nDem; }
  double lat() const { return latF; }
  double lon() const { return lonF; }
  double latitudeBrute() const { return latBrute; }
  double longitudeBrute() const { return lonBrute; }
  bool immobile() const { return immobileFlag; }
  bool signalFaible() const { return signalFaibleFlag; }
  int mesuresMoyennees() const { return immobileFlag ? nMoyenne : 0; }
  bool fige() const { return immobileFlag && nMoyenne >= MESURES_AVANT_FIGEAGE; }
  unsigned long mesuresRejetees() const { return rejetees; }
  unsigned long sauts() const { return nSauts; } // nombre de fois où l'on a "repris" ailleurs
  // Précision d'une mesure seule : HDOP x ~2,5 m avec un bon signal, au moins 25 m avec un signal faible
  float precisionM() const {
    float p = hdopActuel * 2.5f;
    return signalFaibleFlag ? fmaxf(p, 25.0f) : p;
  }

  static float distanceM(double lat1, double lon1, double lat2, double lon2) {
    double dy = (lat1 - lat2) * 110540.0;
    double dx = (lon1 - lon2) * 111320.0 * cos(lat2 * M_PI / 180.0);
    return (float)sqrt(dx * dx + dy * dy);
  }

 private:
  static float poids(float hdop, int satellites) {
    float h = hdop < 0.5f ? 0.5f : hdop;
    return (satellites >= 6 ? 1.0f : 0.5f) / (h * h);
  }

  static double mediane(const double* v, int n) {
    double t[MESURES_DEMARRAGE];
    for (int i = 0; i < n; i++) t[i] = v[i];
    for (int i = 1; i < n; i++) { // tri par insertion (10 valeurs au plus)
      double x = t[i];
      int j = i - 1;
      while (j >= 0 && t[j] > x) { t[j + 1] = t[j]; j--; }
      t[j + 1] = x;
    }
    return n % 2 ? t[n / 2] : (t[n / 2 - 1] + t[n / 2]) / 2;
  }

  // Plus grande distance entre une mesure récente et leur médiane
  float etendueRecente() const {
    double mLat = mediane(recLat, nRec), mLon = mediane(recLon, nRec);
    float m = 0;
    for (int i = 0; i < nRec; i++) m = fmaxf(m, distanceM(recLat[i], recLon[i], mLat, mLon));
    return m;
  }

  void repartirDe(double lat, double lon, unsigned long maintenantMs) {
    latF = lat; lonF = lon;
    immobileFlag = false; nMoyenne = 0; sommePoids = 1;
    rejetsConsecutifs = 0; dernierAccepteMs = maintenantMs;
    precisionAncre = precisionM();
    ancreFaible = signalFaibleFlag;
  }

  bool initialise = false, immobileFlag = false, porteALaMain = false, signalFaibleFlag = false, ancreFaible = false;
  double latF = 0, lonF = 0, latBrute = 0, lonBrute = 0;
  float hdopActuel = 99, precisionAncre = 0;
  int nMoyenne = 0, rejetsConsecutifs = 0;
  double sommePoids = 1;
  double demLat[MESURES_DEMARRAGE] = {}, demLon[MESURES_DEMARRAGE] = {};
  int nDem = 0, iDem = 0;
  unsigned long debutDemarrageMs = 0;
  double recLat[N] = {}, recLon[N] = {};
  int iRec = 0, nRec = 0;
  unsigned long dernierAccepteMs = 0, rejetees = 0, nSauts = 0;
};
