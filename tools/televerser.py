#!/usr/bin/env python3
"""
Téléverser le bon programme sur le bon ESP32, avec les DEUX cartes branchées en
même temps (par exemple via un hub USB sur un seul port du PC).

Chaque XIAO ESP32S3 a un numéro de série unique : Windows lui donne son propre
port COM (ex. COM5 pour l'une, COM7 pour l'autre) et le garde ensuite. Ce script
retrouve lequel est le Robot et lequel est la Manette :
  1. il demande "?" à chaque carte sur son port série -> elle répond ID=ROBOT ou ID=MANETTE ;
  2. sinon il utilise ce qu'il a mémorisé (tools/cartes_esp32.json) ;
  3. sinon (carte neuve) il te demande laquelle est laquelle, une seule fois.

Il utilise l'arduino-cli fourni avec l'Arduino IDE 2 (même carte, mêmes
bibliothèques que dans l'IDE) : rien d'autre à installer que Python.

Usage :
  python tools/televerser.py                  liste les ESP32 branchés et leur rôle
  python tools/televerser.py robot            compile + téléverse robot/robot.ino sur l'ESP32 Robot
  python tools/televerser.py manette          idem pour la Manette
  python tools/televerser.py tout             les deux, l'un après l'autre
  python tools/televerser.py moniteur robot   Moniteur Série du robot (Ctrl+C pour quitter)
  python tools/televerser.py oublier          efface les associations carte <-> rôle mémorisées
Options :
  --port COM5       force le port (sans détection)
  --cli CHEMIN      chemin vers arduino-cli(.exe) si le script ne le trouve pas
  --fqbn FQBN       carte (défaut : esp32:esp32:XIAO_ESP32S3:CDCOnBoot=cdc)
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

try:
    sys.stdout.reconfigure(errors="replace")
except AttributeError:
    pass

DOSSIER_OUTILS = Path(__file__).resolve().parent
RACINE = DOSSIER_OUTILS.parent
FICHIER_CONFIG = DOSSIER_OUTILS / "cartes_esp32.json"
# CDCOnBoot=cdc : le Moniteur Série passe par l'USB natif (nécessaire pour l'identification "?")
FQBN_DEFAUT = "esp32:esp32:XIAO_ESP32S3:CDCOnBoot=cdc"
VID_CONNUS = {0x303A: "ESP32 USB natif", 0x10C4: "CP210x", 0x1A86: "CH340"}
SKETCHS = {"robot": RACINE / "robot", "manette": RACINE / "manette"}
NOMS = {"robot": "ROBOT", "manette": "MANETTE"}
INFOS_GPS = {}  # port -> texte décrivant le GPS du robot (rempli par interroger())
ETATS_GPS = {
    "detection": "détection du module en cours",
    "absent": "AUCUN module GPS trouvé (vérifier le câblage)",
    "illisible": "données reçues mais illisibles (GND commun ? vitesse ?)",
    "recherche": "module OK, recherche des satellites",
    "fix": "position acquise",
    "simulation": "simulation (GPS_SIMULATION = 1)",
}


# ------------------------------------------------------------------ pyserial
def importer_pyserial():
    try:
        import serial  # noqa: F401
        import serial.tools.list_ports  # noqa: F401
    except ImportError:
        print("Installation de la bibliothèque Python « pyserial » (une seule fois)...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "--user", "pyserial"])
        import site
        sys.path.append(site.getusersitepackages())
        import serial  # noqa: F401
        import serial.tools.list_ports  # noqa: F401
    import serial
    return serial


# ------------------------------------------------------------------ mémoire
def charger_config():
    try:
        return json.loads(FICHIER_CONFIG.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def sauver_config(cfg):
    FICHIER_CONFIG.write_text(json.dumps(cfg, indent=2, ensure_ascii=False), encoding="utf-8")


def cle_carte(port_info):
    # Le numéro de série USB (basé sur l'adresse MAC) identifie la carte, quel que soit le port COM.
    return port_info.serial_number or port_info.device


# ------------------------------------------------------------------ détection
def lister_esp32(serial):
    import serial.tools.list_ports
    return sorted((p for p in serial.tools.list_ports.comports() if p.vid in VID_CONNUS), key=lambda p: p.device)


def interroger(serial, port, attente=3.0):
    """Envoie « ? » et attend « ID=ROBOT » / « ID=MANETTE ». Renvoie 'robot', 'manette', None ou 'occupe'."""
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.1
    s.dtr = False  # DTR/RTS au repos : ouvrir le port ne redémarre pas la carte
    s.rts = False
    try:
        s.open()
    except (serial.SerialException, OSError) as e:
        texte = str(e).lower()
        if "access" in texte or "busy" in texte or "denied" in texte or "permission" in texte:
            return "occupe"
        return None
    try:
        recu, fin, dernier_envoi = b"", time.time() + attente, 0.0
        while time.time() < fin:
            if time.time() - dernier_envoi > 0.5:
                s.write(b"?\n")
                dernier_envoi = time.time()
            recu += s.read(512)
            # Ligne complète : "ID=ROBOT GPS=recherche,D7,9600" (le robot décrit aussi son GPS)
            m = re.search(rb"ID=(ROBOT|MANETTE)([^\r\n]*)[\r\n]", recu)
            if m:
                noter_infos_gps(port, m.group(2).decode(errors="replace"))
                return m.group(1).decode().lower()
        m = re.search(rb"ID=(ROBOT|MANETTE)", recu)
        return m.group(1).decode().lower() if m else None
    except (serial.SerialException, OSError):
        return None
    finally:
        s.close()


def noter_infos_gps(port, suite):
    m = re.search(r"GPS=(\w+),([^,\s]+),(\d+)", suite)
    if not m:
        return
    etat, broche, bauds = m.groups()
    texte = "GPS : " + ETATS_GPS.get(etat, etat)
    if broche != "-":
        texte += f" — branché sur {broche} à {bauds} bauds"
    INFOS_GPS[port] = texte


def identifier(serial, cfg):
    """Renvoie une liste de (port_info, rôle ou None, origine)."""
    cartes = cfg.setdefault("cartes", {})
    resultat = []
    for p in lister_esp32(serial):
        role = interroger(serial, p.device)
        if role in NOMS:
            cartes[cle_carte(p)] = role
            resultat.append((p, role, "a répondu"))
        elif role == "occupe":
            resultat.append((p, cartes.get(cle_carte(p)), "port occupé (Moniteur Série ouvert ?)"))
        else:
            memo = cartes.get(cle_carte(p))
            resultat.append((p, memo, "mémorisé" if memo else "inconnue (programme pas encore téléversé ?)"))
    sauver_config(cfg)
    return resultat


def afficher(cartes):
    if not cartes:
        print("Aucun ESP32 détecté.")
        print("  - Vérifie le câble USB (certains câbles ne font que charger) et le hub.")
        print("  - Carte bloquée ? Maintiens BOOT, appuie sur RESET, relâche BOOT, puis réessaie.")
        return
    print(f"{len(cartes)} ESP32 détecté(s) :")
    for p, role, origine in cartes:
        nom = NOMS.get(role, "?")
        print(f"  {p.device:<14} {nom:<8} ({origine})  n° série {p.serial_number or '-'}")
        if p.device in INFOS_GPS:
            print(f"  {'':<14} {INFOS_GPS[p.device]}")


def demander(question, choix):
    while True:
        rep = input(question).strip().lower()
        if rep in choix:
            return rep
        print("   Réponse attendue :", " / ".join(choix))


def choisir_port(serial, role, cfg, port_force=None):
    if port_force:
        return port_force, None
    cartes = identifier(serial, cfg)
    candidats = [c for c in cartes if c[1] == role]
    if len(candidats) == 1:
        return candidats[0][0].device, candidats[0][0]
    afficher(cartes)
    if not cartes:
        sys.exit(1)

    inconnues = [c for c in cartes if c[1] not in NOMS] or cartes
    if len(inconnues) == 1:
        p = inconnues[0][0]
        if demander(f"\nUtiliser {p.device} pour le {NOMS[role]} ? [o/n] ", ["o", "n"]) == "n":
            sys.exit("Abandon. Astuce : débranche l'autre carte, relance, puis rebranche-la.")
        return p.device, p
    print(f"\nPlusieurs cartes possibles pour le {NOMS[role]} :")
    for i, (p, r, _) in enumerate(inconnues, 1):
        print(f"  {i}. {p.device} ({NOMS.get(r, 'inconnue')})")
    print("  Astuce : débranche une carte pour voir quel port disparaît.")
    i = int(demander("Numéro : ", [str(k) for k in range(1, len(inconnues) + 1)]))
    return inconnues[i - 1][0].device, inconnues[i - 1][0]


# ------------------------------------------------------------------ arduino-cli
def trouver_arduino_cli(cfg, chemin_force=None):
    exe = "arduino-cli.exe" if os.name == "nt" else "arduino-cli"
    relatif = Path("resources/app/lib/backend/resources") / exe
    candidats = [chemin_force, cfg.get("arduino_cli"), shutil.which("arduino-cli")]
    for base in (os.environ.get("LOCALAPPDATA", "") + "/Programs/Arduino IDE",
                 os.environ.get("ProgramFiles", "") + "/Arduino IDE",
                 "/Applications/Arduino IDE.app/Contents"):
        candidats.append(str(Path(base) / relatif))
    for c in candidats:
        if c and Path(c).is_file():
            return str(c)
    # Arduino IDE installé ailleurs (ex. dans Documents ou OneDrive) : recherche dans le dossier personnel
    print("Recherche de l'Arduino IDE dans ton dossier personnel...")
    for profondeur in range(0, 5):
        motif = "/".join(["*"] * profondeur + ["Arduino IDE", str(relatif).replace("\\", "/")])
        try:
            for trouve in Path.home().glob(motif):
                cfg["arduino_cli"] = str(trouve)
                sauver_config(cfg)
                return str(trouve)
        except OSError:
            pass
    sys.exit("arduino-cli introuvable. Indique son chemin avec --cli, par exemple :\n"
             '  --cli "C:\\...\\Arduino IDE\\resources\\app\\lib\\backend\\resources\\arduino-cli.exe"')


def options_config_ide():
    # Même configuration que l'Arduino IDE 2 -> mêmes cartes et bibliothèques installées
    fichier = Path.home() / ".arduinoIDE" / "arduino-cli.yaml"
    return ["--config-file", str(fichier)] if fichier.is_file() else []


def televerser(serial, role, cfg, args):
    if role == "robot":
        sys.path.insert(0, str(DOSSIER_OUTILS))
        import generer_ihm_h
        generer_ihm_h.generer()

    cli = trouver_arduino_cli(cfg, args.cli)
    port, info = choisir_port(serial, role, cfg, args.port)
    print(f"\n=== {NOMS[role]} : compilation et téléversement sur {port} ===")
    commande = [cli, *options_config_ide(), "compile", "--fqbn", args.fqbn, "--upload", "-p", port, str(SKETCHS[role])]
    print(" ".join(f'"{c}"' if " " in c else c for c in commande))
    if subprocess.run(commande).returncode != 0:
        print(f"\n[ÉCHEC] {NOMS[role]} non téléversé. Causes fréquentes :")
        print(f"  - le Moniteur Série de l'Arduino IDE est ouvert sur {port} : ferme-le ;")
        print("  - carte bloquée : maintiens BOOT, appuie sur RESET, relâche BOOT, puis relance ;")
        print("  - carte « esp32 » (Espressif) ou bibliothèque « TinyGPSPlus » non installée dans l'IDE.")
        return False

    if info is not None:
        cfg.setdefault("cartes", {})[cle_carte(info)] = role
        sauver_config(cfg)
    time.sleep(2.0)  # redémarrage de la carte
    verif = interroger(serial, port, attente=6.0)
    if verif == role:
        print(f"[OK] {port} répond bien ID={NOMS[role]}.")
    else:
        print(f"[OK] Téléversé sur {port} (la carte n'a pas encore répondu à l'identification, ce n'est pas grave).")
    return True


def moniteur(serial, role, cfg, port_force):
    port, _ = choisir_port(serial, role, cfg, port_force)
    print(f"Moniteur Série du {NOMS[role]} sur {port} (Ctrl+C pour quitter)\n")
    s = serial.Serial()
    s.port, s.baudrate, s.timeout, s.dtr, s.rts = port, 115200, 0.2, False, False
    s.open()
    try:
        while True:
            ligne = s.readline()
            if ligne:
                print(ligne.decode("utf-8", errors="replace").rstrip())
    except KeyboardInterrupt:
        pass
    finally:
        s.close()


# ------------------------------------------------------------------ main
def main():
    parser = argparse.ArgumentParser(description="Téléverse robot.ino / manette.ino sur le bon ESP32.")
    parser.add_argument("action", nargs="?", default="liste", choices=["liste", "robot", "manette", "tout", "moniteur", "oublier"])
    parser.add_argument("cible", nargs="?", choices=["robot", "manette"], help="pour « moniteur »")
    parser.add_argument("--port")
    parser.add_argument("--cli")
    parser.add_argument("--fqbn", default=FQBN_DEFAUT)
    args = parser.parse_args()

    cfg = charger_config()
    if args.action == "oublier":
        cfg.pop("cartes", None)
        sauver_config(cfg)
        print("Associations oubliées.")
        return

    serial = importer_pyserial()
    if args.action == "liste":
        afficher(identifier(serial, cfg))
    elif args.action == "moniteur":
        moniteur(serial, args.cible or "robot", cfg, args.port)
    elif args.action == "tout":
        if args.port:
            sys.exit("--port ne peut pas servir pour « tout » (deux cartes, deux ports).")
        ok = televerser(serial, "robot", cfg, args) and televerser(serial, "manette", cfg, args)
        sys.exit(0 if ok else 1)
    else:
        sys.exit(0 if televerser(serial, args.action, cfg, args) else 1)


if __name__ == "__main__":
    main()
