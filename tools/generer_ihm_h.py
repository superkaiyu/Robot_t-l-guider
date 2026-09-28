#!/usr/bin/env python3
"""Génère robot/ihm_html.h (IHM compressée en gzip) à partir de ihm/ihm.html.

À relancer après chaque modification de ihm/ihm.html, pour que le robot serve
la nouvelle version de l'IHM sur http://192.168.4.1 :

    python tools/generer_ihm_h.py

(tools/televerser.py le fait automatiquement avant de téléverser le robot.)
"""
import gzip
from pathlib import Path

RACINE = Path(__file__).resolve().parent.parent
SOURCE = RACINE / "ihm" / "ihm.html"
CIBLE = RACINE / "robot" / "ihm_html.h"


def generer():
    html = SOURCE.read_bytes()
    # mtime=0 : même HTML -> même fichier généré (pas de faux changements dans git)
    compresse = gzip.compress(html, compresslevel=9, mtime=0)

    lignes = []
    for i in range(0, len(compresse), 20):
        lignes.append("  " + ", ".join(f"0x{b:02x}" for b in compresse[i:i + 20]) + ",")

    CIBLE.write_text(
        "// FICHIER GÉNÉRÉ AUTOMATIQUEMENT par tools/generer_ihm_h.py — ne pas modifier à la main.\n"
        "// Source : ihm/ihm.html (compressée en gzip). Relancer `python tools/generer_ihm_h.py`\n"
        "// après chaque modification de l'IHM.\n"
        "#pragma once\n"
        "#include <pgmspace.h>\n\n"
        f"// {len(html)} octets de HTML -> {len(compresse)} octets compressés\n"
        f"const size_t IHM_HTML_GZ_TAILLE = {len(compresse)};\n"
        "const uint8_t IHM_HTML_GZ[] PROGMEM = {\n" + "\n".join(lignes) + "\n};\n",
        encoding="utf-8",
    )
    print(f"{CIBLE.relative_to(RACINE)} généré : {len(html)} octets de HTML -> {len(compresse)} octets gzip.")


if __name__ == "__main__":
    generer()
