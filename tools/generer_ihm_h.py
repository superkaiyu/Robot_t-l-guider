#!/usr/bin/env python3
"""Génère robot/ihm_html.h à partir de ihm/ihm.html.

À relancer après chaque modification de ihm/ihm.html, pour que le robot serve
la nouvelle version de l'IHM sur http://192.168.4.1 :

    python3 tools/generer_ihm_h.py
"""
from pathlib import Path

RACINE = Path(__file__).resolve().parent.parent
SOURCE = RACINE / "ihm" / "ihm.html"
CIBLE = RACINE / "robot" / "ihm_html.h"
DELIMITEUR = "IHM_HTML_FIN"

html = SOURCE.read_text(encoding="utf-8")
if f"){DELIMITEUR}" in html:
    raise SystemExit(f"Le fichier HTML contient le délimiteur ){DELIMITEUR} : impossible de l'encapsuler.")

CIBLE.write_text(
    "// FICHIER GÉNÉRÉ AUTOMATIQUEMENT par tools/generer_ihm_h.py — ne pas modifier à la main.\n"
    "// Source : ihm/ihm.html. Relancer `python3 tools/generer_ihm_h.py` après chaque modification.\n"
    "#pragma once\n"
    "#include <pgmspace.h>\n\n"
    f'const char IHM_HTML[] PROGMEM = R"{DELIMITEUR}(\n{html}){DELIMITEUR}";\n',
    encoding="utf-8",
)
print(f"{CIBLE.relative_to(RACINE)} généré ({len(html.encode('utf-8'))} octets de HTML).")
