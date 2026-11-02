#!/usr/bin/env python3
"""
Base de données des sessions du robot (SQLite).

Le robot enregistre chaque session (de l'allumage à l'extinction) dans sa mémoire
flash. Ce script les récupère par le WiFi et les range dans une base SQLite sur
le PC (sessions/sessions.db), avec un récapitulatif calculé pour chacune. Les
fichiers CSV d'origine sont gardés à côté (sessions/csv/).

Le PC doit être connecté au WiFi ROBOT_ESP32 pour récupérer les sessions ; la
consultation de la base marche ensuite sans le robot.

Utilisation :
  python tools/sessions.py                récupère les nouvelles sessions puis affiche la liste
  python tools/sessions.py liste          liste des sessions déjà dans la base (sans le robot)
  python tools/sessions.py recap 42       récapitulatif détaillé de la session 42
  python tools/sessions.py importer f.csv importe un CSV (par ex. téléchargé depuis l'IHM)
  python tools/sessions.py exporter 42    réécrit le CSV de la session 42 dans sessions/csv/
Options :
  --robot 192.168.4.1                     adresse du robot
  --base chemin/vers/sessions.db          autre fichier de base de données

La base s'ouvre aussi avec n'importe quel outil SQLite (DB Browser for SQLite...) :
tables "sessions" (une ligne par session, avec le récapitulatif), "mesures" et "evenements".
"""
import argparse
import json
import math
import sqlite3
import sys
import urllib.request
from datetime import datetime
from pathlib import Path

try:
    sys.stdout.reconfigure(errors="replace")
except AttributeError:
    pass

RACINE = Path(__file__).resolve().parent.parent
BASE_DEFAUT = RACINE / "sessions" / "sessions.db"

SCHEMA = """
CREATE TABLE IF NOT EXISTS sessions (
    numero            INTEGER PRIMARY KEY,
    carte             TEXT,
    cause             TEXT,     -- cause du démarrage du robot
    debut             INTEGER,  -- heure de début (secondes depuis 1970), NULL si inconnue
    duree_s           INTEGER,
    nb_mesures        INTEGER,
    distance_m        REAL,     -- distance parcourue d'après le GPS
    vitesse_max_kmh   REAL,
    batterie_debut_v  REAL,
    batterie_fin_v    REAL,
    batterie_min_v    REAL,
    conso_ah          REAL,
    conduite_s        INTEGER,  -- temps avec les moteurs en marche
    conduite_manette_s INTEGER,
    conduite_ihm_s    INTEGER,
    arrets_urgence    INTEGER,
    coupures_ihm      INTEGER,
    deconnexions_wifi INTEGER,
    gps_fix_pourcent  REAL,     -- part du temps avec une position GPS
    satellites_max    INTEGER,
    taille_octets     INTEGER,
    importee_le       TEXT
);
CREATE TABLE IF NOT EXISTS mesures (
    numero INTEGER, t INTEGER, lat REAL, lon REAL, fix INTEGER, sats INTEGER, hdop REAL,
    vitesse_kmh REAL, batterie_v REAL, courant_a REAL, conso_ah REAL,
    pwr_g INTEGER, pwr_d INTEGER, pilote TEXT, ihm INTEGER, manette INTEGER, rssi_min INTEGER
);
CREATE TABLE IF NOT EXISTS evenements (numero INTEGER, t INTEGER, code TEXT, texte TEXT);
CREATE INDEX IF NOT EXISTS idx_mesures ON mesures(numero, t);
CREATE INDEX IF NOT EXISTS idx_evenements ON evenements(numero, t);
"""

COLONNES_MESURES = ["t", "lat", "lon", "fix", "sats", "hdop", "vitesse_kmh", "batterie_v", "courant_a",
                    "conso_ah", "pwr_g", "pwr_d", "pilote", "ihm", "manette", "rssi_min"]
ENTIERS = {"t", "fix", "sats", "pwr_g", "pwr_d", "ihm", "manette", "rssi_min"}


# ------------------------------------------------------------------ lecture du CSV du robot
def lire_csv(texte):
    """Renvoie (infos, mesures, evenements) à partir du CSV écrit par le robot."""
    infos = {"numero": None, "cause": "", "carte": "", "horloge": None}
    mesures, evenements, colonnes = [], [], COLONNES_MESURES
    for ligne in texte.splitlines():
        if not ligne.strip():
            continue
        champs = ligne.split(",")
        if ligne.startswith("#session"):
            infos["numero"] = int(champs[1])
            infos["cause"] = champs[2] if len(champs) > 2 else ""
            infos["carte"] = champs[3] if len(champs) > 3 else ""
        elif ligne.startswith("#horloge"):
            infos["horloge"] = (int(champs[1]), int(champs[2]))  # (heure réelle, t) au même instant
        elif ligne.startswith("#ev"):
            evenements.append((int(champs[1]), champs[2], ",".join(champs[3:])))
        elif ligne.startswith("t,"):
            colonnes = champs
        elif ligne[0].isdigit():
            m = {}
            for nom, valeur in zip(colonnes, champs):
                if valeur == "":
                    m[nom] = None
                elif nom == "pilote":
                    m[nom] = valeur
                else:
                    try:
                        m[nom] = int(valeur) if nom in ENTIERS else float(valeur)
                    except ValueError:
                        m[nom] = None
            mesures.append(m)
    return infos, mesures, evenements


def distance_m(lat1, lon1, lat2, lon2):
    dy = (lat1 - lat2) * 110540.0
    dx = (lon1 - lon2) * 111320.0 * math.cos(math.radians(lat2))
    return math.hypot(dx, dy)


def recapituler(infos, mesures, evenements):
    """Calcule le récapitulatif d'une session (mêmes règles que l'onglet Sessions de l'IHM)."""
    r = {"duree_s": 0, "nb_mesures": len(mesures), "distance_m": 0.0, "vitesse_max_kmh": 0.0,
         "batterie_debut_v": None, "batterie_fin_v": None, "batterie_min_v": None, "conso_ah": 0.0,
         "conduite_s": 0, "conduite_manette_s": 0, "conduite_ihm_s": 0, "gps_fix_pourcent": 0.0, "satellites_max": 0}
    if mesures:
        r["duree_s"] = mesures[-1]["t"] - mesures[0]["t"]
    batteries = [m["batterie_v"] for m in mesures if m.get("batterie_v") is not None and m["batterie_v"] >= 2.0]
    if batteries:
        r["batterie_debut_v"], r["batterie_fin_v"], r["batterie_min_v"] = batteries[0], batteries[-1], min(batteries)
    consos = [m["conso_ah"] for m in mesures if m.get("conso_ah") is not None]
    if consos:
        r["conso_ah"] = max(0.0, consos[-1] - consos[0])

    temps_total = temps_fix = 0
    dernier_point = None
    for i, m in enumerate(mesures):
        r["satellites_max"] = max(r["satellites_max"], m.get("sats") or 0)
        if m.get("fix") and m.get("lat") is not None:
            r["vitesse_max_kmh"] = max(r["vitesse_max_kmh"], m.get("vitesse_kmh") or 0)
            point = (m["lat"], m["lon"])
            # même règle que le tracé : on ne compte un déplacement qu'au-delà de 2 m (bruit du GPS)
            if dernier_point is None:
                dernier_point = point
            elif distance_m(*point, *dernier_point) > 2.0:
                r["distance_m"] += distance_m(*point, *dernier_point)
                dernier_point = point
        if i + 1 < len(mesures):
            dt = mesures[i + 1]["t"] - m["t"]
            if 0 < dt <= 10:  # au-delà, trou dans l'enregistrement : ignoré
                temps_total += dt
                if m.get("fix"):
                    temps_fix += dt
                if (m.get("pwr_g") or 0) != 0 or (m.get("pwr_d") or 0) != 0:
                    r["conduite_s"] += dt
                    if m.get("pilote") == "M":
                        r["conduite_manette_s"] += dt
                    elif m.get("pilote") == "I":
                        r["conduite_ihm_s"] += dt
    r["gps_fix_pourcent"] = round(100.0 * temps_fix / temps_total, 1) if temps_total else 0.0
    codes = [e[1] for e in evenements]
    r["arrets_urgence"] = codes.count("urgence")
    r["coupures_ihm"] = codes.count("ihm-")
    r["deconnexions_wifi"] = codes.count("wifi-")
    r["debut"] = None
    if infos.get("horloge") and mesures:
        epoch, t = infos["horloge"]
        r["debut"] = epoch - t + mesures[0]["t"]
    return r


# ------------------------------------------------------------------ base de données
def ouvrir_base(chemin):
    chemin.parent.mkdir(parents=True, exist_ok=True)
    base = sqlite3.connect(chemin)
    base.executescript(SCHEMA)
    return base


def importer_csv(base, texte, taille=None):
    infos, mesures, evenements = lire_csv(texte)
    if infos["numero"] is None:
        raise ValueError("ce fichier n'est pas une session du robot (pas de ligne #session)")
    n = infos["numero"]
    r = recapituler(infos, mesures, evenements)
    with base:
        base.execute("DELETE FROM mesures WHERE numero = ?", (n,))
        base.execute("DELETE FROM evenements WHERE numero = ?", (n,))
        base.executemany(f"INSERT INTO mesures (numero, {', '.join(COLONNES_MESURES)}) VALUES (?{', ?' * len(COLONNES_MESURES)})",
                         [(n, *[m.get(c) for c in COLONNES_MESURES]) for m in mesures])
        base.executemany("INSERT INTO evenements (numero, t, code, texte) VALUES (?, ?, ?, ?)", [(n, *e) for e in evenements])
        base.execute("""INSERT OR REPLACE INTO sessions (numero, carte, cause, debut, duree_s, nb_mesures, distance_m, vitesse_max_kmh,
                        batterie_debut_v, batterie_fin_v, batterie_min_v, conso_ah, conduite_s, conduite_manette_s, conduite_ihm_s,
                        arrets_urgence, coupures_ihm, deconnexions_wifi, gps_fix_pourcent, satellites_max, taille_octets, importee_le)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                     (n, infos["carte"], infos["cause"], r["debut"], r["duree_s"], r["nb_mesures"], round(r["distance_m"], 1),
                      r["vitesse_max_kmh"], r["batterie_debut_v"], r["batterie_fin_v"], r["batterie_min_v"], round(r["conso_ah"], 4),
                      r["conduite_s"], r["conduite_manette_s"], r["conduite_ihm_s"], r["arrets_urgence"], r["coupures_ihm"],
                      r["deconnexions_wifi"], r["gps_fix_pourcent"], r["satellites_max"], taille or len(texte.encode()),
                      datetime.now().isoformat(timespec="seconds")))
    return n


def recuperer(base, robot, dossier_csv):
    url = f"http://{robot}"
    try:
        with urllib.request.urlopen(f"{url}/sessions", timeout=5) as rep:
            liste = json.loads(rep.read().decode())
    except OSError as e:
        print(f"Robot injoignable sur {url} ({e}).")
        print("Le PC est-il connecté au WiFi ROBOT_ESP32 ? Affichage des sessions déjà dans la base.\n")
        return
    if not liste.get("disponible"):
        print("Le robot n'enregistre pas de sessions : choisir le schéma de partition « Huge APP » dans l'Arduino IDE.")
        return
    deja = dict(base.execute("SELECT numero, taille_octets FROM sessions"))
    nouvelles = 0
    for s in liste["sessions"]:
        n, taille = s["id"], s["taille"]
        if deja.get(n) == taille:
            continue  # déjà à jour
        with urllib.request.urlopen(f"{url}/session?id={n}", timeout=30) as rep:
            donnees = rep.read()
        dossier_csv.mkdir(parents=True, exist_ok=True)
        (dossier_csv / f"session-{n:06d}.csv").write_bytes(donnees)
        importer_csv(base, donnees.decode("utf-8", errors="replace"), taille)
        nouvelles += 1
        etat = " (en cours)" if n == liste.get("enCours") else ""
        print(f"  session {n} récupérée{etat} : {len(donnees) // 1024} Ko")
    print(f"{nouvelles} session(s) récupérée(s), {len(liste['sessions']) - nouvelles} déjà à jour.\n")


# ------------------------------------------------------------------ affichage
def duree(s):
    s = int(s or 0)
    if s >= 3600:
        return f"{s // 3600} h {s % 3600 // 60:02d}"
    return f"{s // 60} min {s % 60:02d} s" if s >= 60 else f"{s} s"


def date(epoch):
    return datetime.fromtimestamp(epoch).strftime("%d/%m/%Y %H:%M") if epoch else "date inconnue"


def volts(v):
    return f"{v:.2f} V".replace(".", ",") if v is not None else "—"


def afficher_liste(base):
    lignes = base.execute("""SELECT numero, debut, duree_s, distance_m, conduite_s, batterie_debut_v, batterie_fin_v,
                             arrets_urgence, coupures_ihm, cause FROM sessions ORDER BY numero DESC""").fetchall()
    if not lignes:
        print("Aucune session dans la base.")
        return
    print(f"{'N°':>5}  {'Début':<17} {'Durée':>11} {'Distance':>9} {'Conduite':>11}  {'Batterie':<17} {'Urg.':>4} {'Coup.':>5}  Démarrage")
    for n, debut, d, dist, cond, b0, b1, urg, coup, cause in lignes:
        batt = f"{volts(b0)} → {volts(b1)}" if b0 is not None else "non branchée"
        print(f"{n:>5}  {date(debut):<17} {duree(d):>11} {dist:>7.0f} m {duree(cond):>11}  {batt:<17} {urg:>4} {coup:>5}  {cause}")


def afficher_recap(base, n):
    s = base.execute("SELECT * FROM sessions WHERE numero = ?", (n,)).fetchone()
    if not s:
        print(f"Session {n} absente de la base.")
        return
    c = [d[0] for d in base.execute("SELECT * FROM sessions LIMIT 0").description]
    s = dict(zip(c, s))
    print(f"Session {n} — {date(s['debut'])} — {s['carte']}")
    print(f"  Démarrage du robot   : {s['cause']}")
    print(f"  Durée                : {duree(s['duree_s'])} ({s['nb_mesures']} mesures)")
    print(f"  Distance (GPS)       : {s['distance_m']:.0f} m, vitesse max {s['vitesse_max_kmh']:.1f} km/h")
    print(f"  Conduite             : {duree(s['conduite_s'])} (manette {duree(s['conduite_manette_s'])}, IHM {duree(s['conduite_ihm_s'])})")
    print(f"  Batterie             : {volts(s['batterie_debut_v'])} → {volts(s['batterie_fin_v'])} (min {volts(s['batterie_min_v'])}), "
          f"{s['conso_ah'] * 1000:.0f} mAh consommés")
    print(f"  GPS                  : position {s['gps_fix_pourcent']:.0f} % du temps, jusqu'à {s['satellites_max']} satellites")
    print(f"  Arrêts d'urgence     : {s['arrets_urgence']}")
    print(f"  Coupures IHM / WiFi  : {s['coupures_ihm']} / {s['deconnexions_wifi']}")
    evenements = base.execute("SELECT t, texte FROM evenements WHERE numero = ? ORDER BY t", (n,)).fetchall()
    if evenements:
        print("  Événements :")
        for t, texte in evenements:
            moment = datetime.fromtimestamp(s["debut"] + t).strftime("%H:%M:%S") if s["debut"] else f"t+{duree(t)}"
            print(f"    {moment:>12}  {texte}")


def exporter(base, n, dossier_csv):
    lignes = base.execute(f"SELECT {', '.join(COLONNES_MESURES)} FROM mesures WHERE numero = ? ORDER BY t", (n,)).fetchall()
    if not lignes:
        print(f"Session {n} absente de la base.")
        return
    dossier_csv.mkdir(parents=True, exist_ok=True)
    chemin = dossier_csv / f"session-{n:06d}-export.csv"
    with open(chemin, "w", encoding="utf-8") as f:
        f.write(",".join(COLONNES_MESURES) + "\n")
        for l in lignes:
            f.write(",".join("" if v is None else str(v) for v in l) + "\n")
    print(f"Exporté : {chemin}")


def main():
    p = argparse.ArgumentParser(description="Base de données des sessions du robot")
    p.add_argument("action", nargs="?", default="recuperer", choices=["recuperer", "liste", "recap", "importer", "exporter"])
    p.add_argument("cible", nargs="?", help="numéro de session (recap, exporter) ou fichier CSV (importer)")
    p.add_argument("--robot", default="192.168.4.1")
    p.add_argument("--base", type=Path, default=BASE_DEFAUT)
    a = p.parse_args()
    base = ouvrir_base(a.base)
    dossier_csv = a.base.parent / "csv"

    if a.action == "recuperer":
        recuperer(base, a.robot, dossier_csv)
        afficher_liste(base)
    elif a.action == "liste":
        afficher_liste(base)
    elif a.action == "recap":
        if not a.cible:
            sys.exit("Préciser le numéro de session : python tools/sessions.py recap 42")
        afficher_recap(base, int(a.cible))
    elif a.action == "importer":
        if not a.cible:
            sys.exit("Préciser le fichier : python tools/sessions.py importer session-42.csv")
        n = importer_csv(base, Path(a.cible).read_text(encoding="utf-8", errors="replace"))
        print(f"Session {n} importée.")
        afficher_recap(base, n)
    elif a.action == "exporter":
        exporter(base, int(a.cible), dossier_csv)


if __name__ == "__main__":
    main()
