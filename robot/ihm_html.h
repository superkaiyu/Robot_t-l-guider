// FICHIER GÉNÉRÉ AUTOMATIQUEMENT par tools/generer_ihm_h.py — ne pas modifier à la main.
// Source : ihm/ihm.html. Relancer `python3 tools/generer_ihm_h.py` après chaque modification.
#pragma once
#include <pgmspace.h>

const char IHM_HTML[] PROGMEM = R"IHM_HTML_FIN(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>IHM Télémétrie et Commande Robot (Roues)</title>
    <!-- Aucune bibliothèque externe : la page fonctionne SANS Internet, quand le PC est
         connecté au WiFi ROBOT_ESP32 (elle est d'ailleurs servie par le robot : http://192.168.4.1). -->
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #1e1e2f; color: #fff; margin: 0; padding: 20px; }
        h1, h2, h3 { text-align: center; color: #4CAF50; }
        h3 { font-size: 1em; margin-bottom: 10px; }
        .main-container { display: flex; flex-direction: column; gap: 20px; }
        .dashboard-grid { display: flex; flex-wrap: wrap; gap: 20px; align-items: flex-start; }
        .panel { background-color: #2a2a40; border-radius: 10px; padding: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); border: 1px solid #3f3f5a; }
        .panel-commande { flex: 1 1 320px; position: sticky; top: 20px; }
        .panel-telemetrie { flex: 2 1 480px; min-width: 0; }

        .tabs-nav { display: flex; gap: 10px; margin-bottom: 20px; border-bottom: 2px solid #3f3f5a; padding-bottom: 10px; overflow-x: auto; }
        .tab-btn { background-color: #1e1e2f; color: #ccc; border: 1px solid #3f3f5a; padding: 10px 20px; border-radius: 6px; cursor: pointer; font-size: 1em; font-weight: bold; white-space: nowrap; }
        .tab-btn:hover { background-color: #3f3f5a; color: white; }
        .tab-btn.active { background-color: #4CAF50; color: white; border-color: #4CAF50; }
        .tab-pane { display: none; }
        .tab-pane.active { display: block; }

        .panel-connexion { display: flex; flex-wrap: wrap; gap: 25px; align-items: center; justify-content: space-between; }
        .conn-group { display: flex; align-items: center; gap: 10px; flex: 1 1 260px; }
        .conn-group input[type="text"] { background-color: #1e1e2f; border: 1px solid #3f3f5a; color: #fff; padding: 8px 10px; border-radius: 5px; width: 150px; }
        .reseau-info { flex: 1 1 100%; font-size: 0.8em; color: #999; border-top: 1px solid #3f3f5a; padding-top: 10px; }
        .status-dot { width: 12px; height: 12px; border-radius: 50%; display: inline-block; background-color: #666; flex-shrink: 0; }
        .status-dot.ok { background-color: #4CAF50; } .status-dot.warn { background-color: #FF9800; } .status-dot.error { background-color: #f44336; }
        .status-text { font-size: 0.9em; color: #ccc; }
        button.btn-primary { background-color: #4CAF50; color: white; border: none; padding: 10px 16px; border-radius: 5px; cursor: pointer; font-size: 0.95em; }
        button.btn-primary.connected { background-color: #f44336; }

        .kpi-container { display: flex; flex-wrap: wrap; justify-content: space-between; gap: 15px; margin-bottom: 20px; }
        .kpi-card { background-color: #1e1e2f; border-radius: 8px; padding: 15px; flex: 1 1 100px; text-align: center; border: 1px solid #3f3f5a; }
        .kpi-value { font-size: 1.8em; font-weight: bold; color: #4CAF50; margin: 10px 0; overflow-wrap: anywhere; }
        .kpi-value.perime { color: #777 !important; }

        .telemetrie-section { padding-top: 15px; margin-top: 15px; }
        .telemetrie-section:first-of-type { padding-top: 0; margin-top: 0; }
        .section-title { text-align: left; font-size: 1.1em; margin: 0 0 10px 0; padding-left: 10px; border-left: 4px solid #4CAF50; }
        .section-title-bms { border-left-color: #00BCD4; color: #00BCD4; }
        .section-title-roue-g { border-left-color: #2196F3; color: #2196F3; }
        .section-title-roue-d { border-left-color: #FF9800; color: #FF9800; }
        .section-title-gps { border-left-color: #8BC34A; color: #8BC34A; }

        .bms-alerte { display: none; background-color: rgba(244, 67, 54, 0.15); border: 1px solid #f44336; color: #ff8a80; border-radius: 6px; padding: 10px 14px; margin-bottom: 15px; font-size: 0.9em; }
        .bms-alerte.visible { display: block; }
        .badge-etat-bms { font-size: 0.6em; padding: 2px 10px; border-radius: 10px; margin-left: 8px; vertical-align: middle; font-weight: bold; }
        .badge-etat-bms.ok { background: #4CAF50; color: #fff; } .badge-etat-bms.alerte { background: #f44336; color: #fff; }

        .gamepad-panel { background-color: #1e1e2f; border-radius: 8px; padding: 15px; border: 1px solid #3f3f5a; margin-bottom: 20px; text-align: center; }
        .stick-box { width: 90px; height: 90px; border: 2px solid #3f3f5a; border-radius: 8px; position: relative; background: #14141f; margin: 12px auto; }
        .stick-box::before, .stick-box::after { content: ''; position: absolute; background: #2a2a40; }
        .stick-box::before { left: 50%; top: 0; bottom: 0; width: 1px; } .stick-box::after { top: 50%; left: 0; right: 0; height: 1px; }
        .stick-dot { width: 14px; height: 14px; border-radius: 50%; background: #4CAF50; position: absolute; left: 50%; top: 50%; transform: translate(-50%, -50%); z-index: 1; }
        .gamepad-help { font-size: 0.78em; color: #999; text-align: left; margin-top: 10px; line-height: 1.5; }

        .d-pad-container { display: flex; flex-direction: column; align-items: center; margin-bottom: 20px; }
        .d-pad { display: grid; grid-template-columns: repeat(3, 60px); grid-template-rows: repeat(3, 50px); gap: 8px; }
        .d-pad button { font-size: 20px; border-radius: 10px; border: none; cursor: pointer; background-color: #3f3f5a; color: white; }
        .d-pad button:active { background-color: #4CAF50; transform: scale(0.95); }
        .btn-up { grid-column: 2; grid-row: 1; } .btn-left { grid-column: 1; grid-row: 2; }
        .btn-stop-mini { grid-column: 2; grid-row: 2; background-color: #f44336 !important; }
        .btn-right { grid-column: 3; grid-row: 2; } .btn-down { grid-column: 2; grid-row: 3; }
        .btn-stop-central { margin-top: 16px; padding: 10px 20px; font-size: 1em; border-radius: 8px; border: none; cursor: pointer; background-color: #f44336; color: white; width: 100%; }

        .control-group { margin-top: 15px; }
        input[type="range"] { width: 100%; margin-top: 5px; accent-color: #4CAF50; }
        .json-monitor { background-color: #000; color: #0f0; font-family: monospace; padding: 10px; border-radius: 5px; margin-top: 5px; min-height: 20px; display: flex; align-items: center; word-break: break-all; }
        .mode-label { font-size: 0.85em; color: #999; margin-top: 8px; }
        .mode-label span { color: #FF9800; font-weight: bold; }

        .charts-container { display: flex; flex-wrap: wrap; gap: 20px; margin-top: 20px; }
        .chart-box { background-color: #1e1e2f; border-radius: 8px; padding: 15px; flex: 1 1 260px; border: 1px solid #3f3f5a; min-width: 0; }
        .chart-box canvas { display: block; width: 100%; height: auto; }

        .panel-trace-header { display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; gap: 10px; margin-top: 20px; padding-top: 20px; border-top: 1px solid #3f3f5a; }
        .trace-canvas { display: block; width: 100%; max-width: 700px; margin: 15px auto 5px auto; background-color: #14141f; border: 1px solid #3f3f5a; border-radius: 8px; }
        .trace-scale-label { text-align: center; font-size: 0.85em; color: #999; }
        button.btn-secondary { background-color: #3f3f5a; color: white; border: none; padding: 8px 14px; border-radius: 5px; cursor: pointer; font-size: 0.9em; }
        a.btn-secondary { background-color: #3f3f5a; color: white; padding: 8px 14px; border-radius: 5px; font-size: 0.9em; text-decoration: none; display: inline-block; }

        .note-box { font-size: 0.8em; color: #999; background: #1e1e2f; border: 1px dashed #3f3f5a; border-radius: 6px; padding: 10px 14px; margin-top: 15px; line-height: 1.5; }
        .gps-resume { cursor: pointer; }
        .gps-details { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 6px 20px; font-size: 0.85em; color: #ccc; background: #1e1e2f; border: 1px solid #3f3f5a; border-radius: 8px; padding: 12px 15px; }
        .gps-details b { color: #fff; }
        .gps-actions { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 15px; align-items: center; }
        .gps-carte { width: 100%; height: 320px; border: 1px solid #3f3f5a; border-radius: 8px; margin-top: 15px; display: none; background: #14141f; }
        .gps-carte.visible { display: block; }

        /* Téléphone : l'IHM est aussi utilisable depuis un smartphone connecté à ROBOT_ESP32 */
        @media (max-width: 600px) {
            body { padding: 10px; }
            h1 { font-size: 1.4em; }
            .panel { padding: 12px; }
            .panel-commande { position: static; flex-basis: 100%; min-width: 0; }
            .panel-telemetrie { flex-basis: 100%; }
            .conn-group { flex-wrap: wrap; flex-basis: 100%; }
            .conn-group input[type="text"] { flex: 1 1 120px; width: auto; }
            .kpi-card { flex-basis: 80px; padding: 10px; }
            .kpi-value { font-size: 1.4em; }
            .tab-btn { padding: 8px 12px; }
        }
    </style>
</head>
<body>

    <h1>Interface de Pilotage — Roues (ESP32 XIAO S3)</h1>

    <div class="main-container">
        <div class="panel panel-connexion">
            <div class="conn-group">
                <span class="status-dot" id="dot-esp32"></span>
                <div>
                    <div><strong>Robot (ESP32 WiFi)</strong></div>
                    <div class="status-text" id="label-esp32-status">Déconnecté</div>
                </div>
                <input type="text" id="ip-esp32" placeholder="192.168.4.1">
                <button class="btn-primary" id="btn-connect-esp32">Se connecter</button>
            </div>
            <div class="conn-group">
                <span class="status-dot" id="dot-manette-esp32"></span>
                <div>
                    <div><strong>ESP32 Manette (joystick)</strong></div>
                    <div class="status-text" id="label-manette-esp32">Inconnue (robot non connecté)</div>
                </div>
            </div>
            <div class="conn-group">
                <span class="status-dot" id="dot-gps"></span>
                <div>
                    <div><strong>GPS (sur le robot)</strong></div>
                    <div class="status-text" id="label-gps-liaison">Inconnu (robot non connecté)</div>
                </div>
            </div>
            <div class="conn-group">
                <span class="status-dot" id="dot-gamepad"></span>
                <div>
                    <div><strong>Manette USB (PC)</strong></div>
                    <div class="status-text" id="label-gamepad-status">Aucune manette détectée</div>
                </div>
            </div>
            <div class="reseau-info" id="reseau-info">Réseau : connectez ce PC au WiFi <strong>ROBOT_ESP32</strong> (mot de passe motdepasse123), puis ouvrez <strong>http://192.168.4.1</strong>.</div>
        </div>

        <div class="dashboard-grid">
            <div class="panel panel-commande">
                <h2>Commande</h2>

                <div class="gamepad-panel">
                    <h3>Joystick physique (ESP32 Manette, en WiFi)</h3>
                    <div style="text-align:center; font-size:0.8em; color:#9C27B0; margin-bottom:4px;" id="label-joystick-source">Position en direct (relayée par le robot)</div>
                    <div class="stick-box"><div class="stick-dot" id="stick-dot-physique" style="background:#9C27B0;"></div></div>
                    <div style="text-align:center; font-size:0.85em; margin-top:8px; color:#ccc;">
                        Bouton : <span id="label-joystick-bouton" style="font-weight:bold; color:#4CAF50;">Relâché</span>
                    </div>
                    <div style="text-align:center; font-size:0.75em; color:#666; margin-top:4px;" id="label-joystick-valeurs">X: 0.00 &nbsp; Y: 0.00</div>
                    <div class="gamepad-help">Bouton du joystick = <strong>arrêt d'urgence</strong> du robot (prioritaire sur tout, IHM comprise).</div>
                </div>

                <div class="gamepad-panel">
                    <h3>Manette USB (branchée au PC)</h3>
                    <div style="text-align:center; font-size:0.8em; color:#2196F3; margin-bottom:4px;">Déplacement (stick gauche)</div>
                    <div class="stick-box"><div class="stick-dot" id="stick-dot-roues" style="background:#2196F3;"></div></div>
                    <div class="gamepad-help">
                        <strong>Stick gauche</strong> : haut/bas = avancer/reculer, gauche/droite = tourner<br>
                        <strong>Croix numérique</strong> (si le stick est au repos) : déplacement à puissance fixe<br>
                        <strong>Bouton B / Rond</strong> : arrêt d'urgence
                    </div>
                </div>

                <div class="d-pad-container">
                    <h3>Secours (souris / tactile)</h3>
                    <div class="d-pad">
                        <button class="btn-up" id="cmd-roue-avant">⬆️</button>
                        <button class="btn-left" id="cmd-roue-gauche">⬅️</button>
                        <button class="btn-stop-mini" id="cmd-roue-stop">🛑</button>
                        <button class="btn-right" id="cmd-roue-droite">➡️</button>
                        <button class="btn-down" id="cmd-roue-arriere">⬇️</button>
                    </div>
                    <button class="btn-stop-central" id="cmd-stop-global">🛑 Arrêt d'urgence</button>
                </div>

                <div class="control-group">
                    <label>Puissance Électrique Max : <span id="label-speed">50</span>%</label>
                    <input type="range" id="slider-speed" min="0" max="100" value="50">
                </div>

                <div class="control-group">
                    <label>Trame envoyée à l'ESP32 :</label>
                    <div class="json-monitor" id="json-out">/cmd?src=ihm&rg=0&rd=0</div>
                    <div class="mode-label">Mode : <span id="mode-label">Démo (simulation locale)</span></div>
                    <div class="mode-label">Qui pilote le robot : <span id="pilote-label" style="color:#999;">—</span></div>
                </div>

                <div class="note-box">Priorité : arrêt d'urgence &gt; ESP32 Manette &gt; cette IHM. Une source au repos ne bloque jamais les autres. Config actuelle : roues seulement, batterie mesurée globalement (pack).</div>
            </div>

            <div class="panel panel-telemetrie">
                <div class="tabs-nav">
                    <button class="tab-btn active" data-tab="tab-global">Global & Tracé</button>
                    <button class="tab-btn" data-tab="tab-gps">GPS</button>
                    <button class="tab-btn" data-tab="tab-bms">Batterie</button>
                    <button class="tab-btn" data-tab="tab-roues">Roues</button>
                </div>

                <div id="tab-global" class="tab-pane active">
                    <div class="kpi-container">
                        <div class="kpi-card"><div>Vitesse (Moyenne)</div><div class="kpi-value" id="val-vitesse-globale">0.00</div><div>m/s</div></div>
                        <div class="kpi-card"><div>Distance Totale</div><div class="kpi-value" id="val-distance">0.00</div><div>mètres</div></div>
                        <div class="kpi-card"><div>Batterie (pack)</div><div class="kpi-value" id="val-batterie" style="color:#2196F3;">12.00</div><div>Volts</div></div>
                        <div class="kpi-card"><div>État de charge</div><div class="kpi-value" id="val-soc" style="color:#00BCD4;">100</div><div>%</div></div>
                        <div class="kpi-card"><div>Consommation</div><div class="kpi-value" id="val-consommation" style="color:#E91E63;">0.000</div><div>Ah cumulés</div></div>
                    </div>
                    <div class="control-group">
                        <label>Simuler une charge mécanique (mode démo uniquement) : <span id="label-slope">0</span>%</label>
                        <input type="range" id="slider-slope" min="0" max="100" value="0">
                    </div>

                    <div class="telemetrie-section gps-resume" id="gps-resume" title="Voir l'onglet GPS">
                        <h3 class="section-title section-title-gps">
                            Position GPS
                            <span class="badge-etat-bms" data-gps="badge" style="background:#666;">Robot non connecté</span>
                        </h3>
                        <div class="kpi-container">
                            <div class="kpi-card"><div>Latitude</div><div class="kpi-value" data-gps="lat" style="color:#8BC34A;font-size:1.3em;">--</div></div>
                            <div class="kpi-card"><div>Longitude</div><div class="kpi-value" data-gps="lon" style="color:#8BC34A;font-size:1.3em;">--</div></div>
                            <div class="kpi-card"><div>Satellites</div><div class="kpi-value" data-gps="sat">0</div><div style="font-size:0.8em;color:#999;">utilisés / visibles</div></div>
                        </div>
                    </div>

                    <div class="charts-container">
                        <div class="chart-box"><canvas id="globalSpeedChart" width="400" height="200"></canvas></div>
                        <div class="chart-box"><canvas id="consumptionChart" width="400" height="200"></canvas></div>
                    </div>
                    <div class="panel-trace-header">
                        <h2 style="margin:0;">Tracé du robot (odométrie)</h2>
                        <button class="btn-secondary" id="btn-reset-trace">Réinitialiser le tracé</button>
                    </div>
                    <canvas id="traceCanvas" class="trace-canvas" width="700" height="400"></canvas>
                    <div class="trace-scale-label" id="trace-scale-label">Position : (0.00, 0.00) m — cap 0°</div>
                </div>

                <div id="tab-gps" class="tab-pane">
                    <h3 class="section-title section-title-gps">
                        Position GPS du robot
                        <span class="badge-etat-bms" data-gps="badge" style="background:#666;">Robot non connecté</span>
                    </h3>
                    <div class="kpi-container">
                        <div class="kpi-card"><div>Latitude</div><div class="kpi-value" data-gps="lat" style="color:#8BC34A;font-size:1.3em;">--</div></div>
                        <div class="kpi-card"><div>Longitude</div><div class="kpi-value" data-gps="lon" style="color:#8BC34A;font-size:1.3em;">--</div></div>
                        <div class="kpi-card"><div>Satellites</div><div class="kpi-value" data-gps="sat">0</div><div style="font-size:0.8em;color:#999;">utilisés / visibles</div></div>
                        <div class="kpi-card"><div>Vitesse GPS</div><div class="kpi-value" id="val-gps-vitesse" style="font-size:1.3em;">--</div><div>km/h</div></div>
                        <div class="kpi-card"><div>Altitude</div><div class="kpi-value" id="val-gps-altitude" style="font-size:1.3em;">--</div><div>m</div></div>
                    </div>
                    <div class="gps-details">
                        <div>Module : <b id="val-gps-module">—</b></div>
                        <div>Précision (HDOP) : <b id="val-gps-hdop">—</b></div>
                        <div>Cap : <b id="val-gps-cap">—</b></div>
                        <div>Heure UTC (GPS) : <b id="val-gps-heure">—</b></div>
                        <div>Caractères reçus : <b id="val-gps-caracteres">0</b></div>
                        <div>Trames valides / erronées : <b id="val-gps-trames">0 / 0</b></div>
                    </div>
                    <div class="note-box" id="note-gps">Connectez-vous au robot pour recevoir la position GPS.</div>

                    <div class="gps-actions">
                        <a class="btn-secondary" id="lien-osm" href="#" target="_blank" rel="noopener">Ouvrir dans OpenStreetMap</a>
                        <a class="btn-secondary" id="lien-gmaps" href="#" target="_blank" rel="noopener">Ouvrir dans Google Maps</a>
                        <label style="font-size:0.85em;color:#ccc;"><input type="checkbox" id="chk-carte"> Afficher la carte (nécessite Internet sur ce PC)</label>
                    </div>
                    <iframe class="gps-carte" id="gps-carte" title="Carte OpenStreetMap"></iframe>

                    <div class="panel-trace-header">
                        <h2 style="margin:0;">Trajet GPS</h2>
                        <button class="btn-secondary" id="btn-reset-trace-gps">Réinitialiser le trajet</button>
                    </div>
                    <canvas id="traceGpsCanvas" class="trace-canvas" width="700" height="400"></canvas>
                    <div class="trace-scale-label" id="trace-gps-label">En attente d'une position GPS...</div>
                </div>

                <div id="tab-bms" class="tab-pane">
                    <h3 class="section-title section-title-bms">
                        Batterie (mesure globale du pack)
                        <span class="badge-etat-bms ok" id="badge-etat-bms">OK</span>
                    </h3>
                    <div class="bms-alerte" id="bms-alerte">⚠️ <strong id="bms-alerte-texte">Protection déclenchée</strong></div>
                    <div class="kpi-container">
                        <div class="kpi-card"><div>Tension pack</div><div class="kpi-value" id="val-batterie-2" style="color:#2196F3;">12.00</div><div>Volts</div></div>
                        <div class="kpi-card"><div>Courant</div><div class="kpi-value" id="val-courant" style="color:#FF9800;">0.20</div><div>Ampères</div></div>
                        <div class="kpi-card"><div>SoC</div><div class="kpi-value" id="val-soc-2" style="color:#00BCD4;">100</div><div>%</div></div>
                    </div>
                    <div class="charts-container">
                        <div class="chart-box"><canvas id="batteryChart" width="400" height="200"></canvas></div>
                        <div class="chart-box"><canvas id="socChart" width="400" height="200"></canvas></div>
                        <div class="chart-box"><canvas id="currentChart" width="400" height="200"></canvas></div>
                    </div>
                    <div class="note-box">Pas de mesure cellule par cellule sur cette configuration (une seule broche ADC batterie). Un déséquilibre entre cellules ne serait pas détecté ici.</div>
                </div>

                <div id="tab-roues" class="tab-pane">
                    <div class="telemetrie-section">
                        <h3 class="section-title section-title-roue-g">Roue Gauche</h3>
                        <div class="kpi-container">
                            <div class="kpi-card"><div>Vitesse</div><div class="kpi-value" id="val-vitesse-roue-g">0.00</div><div>m/s</div></div>
                            <div class="kpi-card"><div>Distance</div><div class="kpi-value" id="val-distance-roue-g" style="color:#2196F3;">0.00</div><div>mètres</div></div>
                        </div>
                        <div class="charts-container">
                            <div class="chart-box"><canvas id="speedChartRoueG" width="400" height="200"></canvas></div>
                            <div class="chart-box"><canvas id="distanceChartRoueG" width="400" height="200"></canvas></div>
                        </div>
                    </div>
                    <div class="telemetrie-section">
                        <h3 class="section-title section-title-roue-d">Roue Droite</h3>
                        <div class="kpi-container">
                            <div class="kpi-card"><div>Vitesse</div><div class="kpi-value" id="val-vitesse-roue-d">0.00</div><div>m/s</div></div>
                            <div class="kpi-card"><div>Distance</div><div class="kpi-value" id="val-distance-roue-d" style="color:#FF9800;">0.00</div><div>mètres</div></div>
                        </div>
                        <div class="charts-container">
                            <div class="chart-box"><canvas id="speedChartRoueD" width="400" height="200"></canvas></div>
                            <div class="chart-box"><canvas id="distanceChartRoueD" width="400" height="200"></canvas></div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        // ==================== GRAPHIQUES (sans bibliothèque, fonctionne hors Internet) ====================
        const maxDataPoints = 50;
        class MiniChart {
            constructor(id, label, couleur, opts = {}) {
                this.canvas = document.getElementById(id);
                this.ctx = this.canvas.getContext('2d');
                this.label = label; this.couleur = couleur;
                this.min = opts.min; this.max = opts.max;
                this.data = Array(maxDataPoints).fill(opts.initial ?? 0);
                this.aDessiner = true;
            }
            push(v) { this.data.push(v); this.data.shift(); this.aDessiner = true; }
            dessiner() {
                this.aDessiner = false;
                const ctx = this.ctx, w = this.canvas.width, h = this.canvas.height;
                const g = 42, d = 8, haut = 28, bas = 8;
                ctx.clearRect(0, 0, w, h);
                let min = this.min ?? Math.min(0, ...this.data), max = this.max ?? Math.max(...this.data);
                if (max - min < 1e-6) max = min + 1;
                const y = v => haut + (h - haut - bas) * (1 - (Math.min(max, Math.max(min, v)) - min) / (max - min));
                ctx.font = '11px sans-serif'; ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
                for (let i = 0; i <= 4; i++) {
                    const v = min + (max - min) * i / 4, py = y(v);
                    ctx.strokeStyle = '#3f3f5a'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(g, py); ctx.lineTo(w - d, py); ctx.stroke();
                    const dec = (max - min) < 0.05 ? 4 : (max - min) < 5 ? 2 : 0;
                    ctx.fillStyle = '#aaa'; ctx.fillText(v.toFixed(dec), g - 6, py);
                }
                ctx.strokeStyle = this.couleur; ctx.lineWidth = 2; ctx.beginPath();
                this.data.forEach((v, i) => { const px = g + (w - g - d) * i / (this.data.length - 1); i === 0 ? ctx.moveTo(px, y(v)) : ctx.lineTo(px, y(v)); });
                ctx.stroke();
                ctx.fillStyle = this.couleur; ctx.fillRect(g, 8, 12, 12);
                ctx.fillStyle = '#fff'; ctx.textAlign = 'left'; ctx.font = '13px sans-serif';
                ctx.fillText(`${this.label} : ${Number(this.data[this.data.length - 1]).toFixed(3)}`, g + 18, 14);
            }
        }
        const creerChartSimple = (id, label, couleur, opts) => new MiniChart(id, label, couleur, opts);

        const globalSpeedChart = creerChartSimple('globalSpeedChart', 'Vitesse globale (m/s)', '#4CAF50');
        const consumptionChart = creerChartSimple('consumptionChart', 'Consommation cumulée (Ah)', '#E91E63');
        const batteryChart = creerChartSimple('batteryChart', 'Tension pack (V)', '#2196F3', { min: 9, max: 13, initial: 12.3 });
        const socChart = creerChartSimple('socChart', 'SoC (%)', '#00BCD4', { min: 0, max: 100, initial: 100 });
        const currentChart = creerChartSimple('currentChart', 'Courant (A)', '#FF9800');
        const speedChartRoueG = creerChartSimple('speedChartRoueG', 'Vitesse Roue G (m/s)', '#2196F3');
        const distanceChartRoueG = creerChartSimple('distanceChartRoueG', 'Distance Roue G (m)', '#90CAF9');
        const speedChartRoueD = creerChartSimple('speedChartRoueD', 'Vitesse Roue D (m/s)', '#FF9800');
        const distanceChartRoueD = creerChartSimple('distanceChartRoueD', 'Distance Roue D (m)', '#FFCC80');
        const tousLesCharts = [globalSpeedChart, consumptionChart, batteryChart, socChart, currentChart, speedChartRoueG, distanceChartRoueG, speedChartRoueD, distanceChartRoueD];

        // On ne redessine que les graphiques visibles, au rythme de l'écran.
        function boucleDessinCharts() {
            tousLesCharts.forEach(c => { if (c.aDessiner && c.canvas.offsetParent !== null) c.dessiner(); });
            requestAnimationFrame(boucleDessinCharts);
        }
        requestAnimationFrame(boucleDessinCharts);

        // ==================== ONGLETS ====================
        function switchTab(tabId) {
            document.querySelectorAll('.tab-pane').forEach(el => el.classList.toggle('active', el.id === tabId));
            document.querySelectorAll('.tab-btn').forEach(el => el.classList.toggle('active', el.dataset.tab === tabId));
            tousLesCharts.forEach(c => c.aDessiner = true);
            if (tabId === 'tab-gps') dessinerTraceGPS();
        }
        document.querySelectorAll('.tab-btn').forEach(btn => btn.addEventListener('click', () => switchTab(btn.dataset.tab)));
        document.getElementById('gps-resume').addEventListener('click', () => switchTab('tab-gps'));

        // ==================== ÉTAT GLOBAL ====================
        let esp32Url = null, httpConnecte = false, derniereReponseOkMs = 0, tentativeConnexionDepuisMs = 0;
        let cmdEnCours = false, telemetrieEnCours = false, telemetrieTimer = null;
        const HTTP_TIMEOUT_MS = 800, HTTP_PERTE_LIAISON_MS = 2000, INTERVALLE_TELEMETRIE_MS = 200;
        let derniereCmdEnvoyee = null, derniereCmdEnvoyeeMs = 0;
        const INTERVALLE_CMD_REPOS_MS = 1000; // au repos, un "0,0" par seconde suffit

        let gamepadIndex = null;
        let gamepadRoueAvance = 0, gamepadRoueTourne = 0;
        let gamepadCroixRoueAvance = 0, gamepadCroixRoueTourne = 0;
        let manuelRoueAvance = 0, manuelRoueTourne = 0;

        let vitesseActuelleRoueG = 0, vitesseActuelleRoueD = 0;
        let distanceCumuleeRoueG = 0, distanceCumuleeRoueD = 0;
        let distanceTotale = 0, consommationTotale = 0;
        let tensionPack = 12.3, courantPack = 0.2, socPack = 100;
        let etatBMS = 'OK';
        const SEUIL_SOUS_TENSION_PACK = 9.0, SEUIL_SURTENSION_PACK = 12.6, SEUIL_SURINTENSITE = 10.0;

        function socDepuisTension(v) {
            const points = [[3.00,0],[3.30,5],[3.50,10],[3.60,20],[3.65,30],[3.70,40],[3.75,50],[3.80,60],[3.90,70],[4.00,80],[4.10,90],[4.20,100]];
            if (v <= points[0][0]) return 0;
            if (v >= points[points.length-1][0]) return 100;
            for (let i=0;i<points.length-1;i++){const [v1,s1]=points[i],[v2,s2]=points[i+1]; if (v>=v1&&v<=v2) return s1+(s2-s1)*(v-v1)/(v2-v1);}
            return 0;
        }

        const intervalleUpdateMs = 100, dt = intervalleUpdateMs/1000, DEADZONE = 0.15, VITESSE_MAX_ROUE = 2.5, ENTRAXE_M = 0.6;
        let posX=0,posY=0,heading=0,chemin=[{x:0,y:0}]; const MAX_POINTS_TRACE=4000; let dernierTelemetryTimestamp=null;

        const sliderSpeed = document.getElementById('slider-speed');
        const sliderSlope = document.getElementById('slider-slope');
        const labelSpeed = document.getElementById('label-speed');
        const labelSlope = document.getElementById('label-slope');
        const jsonOut = document.getElementById('json-out');
        const modeLabel = document.getElementById('mode-label');
        const piloteLabel = document.getElementById('pilote-label');
        const ipInput = document.getElementById('ip-esp32');
        const btnConnect = document.getElementById('btn-connect-esp32');
        const dotEsp32 = document.getElementById('dot-esp32');
        const labelEsp32Status = document.getElementById('label-esp32-status');
        const dotManetteEsp32 = document.getElementById('dot-manette-esp32');
        const labelManetteEsp32 = document.getElementById('label-manette-esp32');
        const dotGps = document.getElementById('dot-gps');
        const labelGpsLiaison = document.getElementById('label-gps-liaison');
        const reseauInfo = document.getElementById('reseau-info');
        const dotGamepad = document.getElementById('dot-gamepad');
        const labelGamepadStatus = document.getElementById('label-gamepad-status');
        const stickDotRoues = document.getElementById('stick-dot-roues');
        const stickDotPhysique = document.getElementById('stick-dot-physique');
        const labelJoystickBouton = document.getElementById('label-joystick-bouton');
        const labelJoystickValeurs = document.getElementById('label-joystick-valeurs');
        const labelJoystickSource = document.getElementById('label-joystick-source');

        sliderSpeed.addEventListener('input', e => labelSpeed.textContent = e.target.value);
        sliderSlope.addEventListener('input', e => labelSlope.textContent = e.target.value);

        // ==================== AFFICHAGE : JOYSTICK, LIAISONS, PILOTE ====================
        function afficherJoystickPhysique(x, y, bouton, manetteConnectee) {
            const xClamp = Math.max(-1, Math.min(1, x));
            const yClamp = Math.max(-1, Math.min(1, y));
            stickDotPhysique.style.left = `${50 + xClamp * 40}%`;
            stickDotPhysique.style.top = `${50 + yClamp * 40}%`;
            stickDotPhysique.style.opacity = manetteConnectee ? '1' : '0.3';
            labelJoystickSource.textContent = manetteConnectee ? 'Position en direct (relayée par le robot)' : 'ESP32 Manette non connectée au robot';
            labelJoystickSource.style.color = manetteConnectee ? '#9C27B0' : '#f44336';
            labelJoystickBouton.textContent = bouton ? 'Appuyé (arrêt d’urgence)' : 'Relâché';
            labelJoystickBouton.style.color = bouton ? '#f44336' : '#4CAF50';
            labelJoystickValeurs.textContent = `X: ${x.toFixed(2)}   Y: ${y.toFixed(2)}`;
        }

        const NOMS_PILOTES = {
            'aucun': ['Personne (robot à l’arrêt)', '#999'],
            'manette': ['ESP32 Manette (joystick)', '#9C27B0'],
            'ihm': ['Cette IHM', '#2196F3'],
            'joystick-local': ['Joystick local du robot', '#FF9800'],
            'arret-urgence': ['ARRÊT D’URGENCE', '#f44336'],
        };
        function afficherPilote(pilote) {
            const [texte, couleur] = NOMS_PILOTES[pilote] || ['—', '#999'];
            piloteLabel.textContent = texte; piloteLabel.style.color = couleur;
        }

        function afficherLiaisons(data) {
            if (data.manetteConnectee !== undefined) {
                const ok = !!data.manetteConnectee;
                dotManetteEsp32.className = 'status-dot ' + (ok ? 'ok' : 'error');
                labelManetteEsp32.textContent = ok ? `Connectée au robot (signal ${Number(data.manetteRssi) || 0} dBm)` : 'Non connectée au robot (allumée ?)';
            }
            if (data.apClients !== undefined) {
                let t = `Point d'accès ROBOT_ESP32 : ${data.apClients} appareil(s) connecté(s)`;
                t += data.staConnecte ? ` · Robot aussi sur « ${data.staSsid} » (http://${data.staIp})` : ' · Pas de réseau externe';
                if (data.uptime !== undefined) t += ` · Robot allumé depuis ${Math.floor(data.uptime / 60)} min ${data.uptime % 60} s`;
                reseauInfo.textContent = t;
            }
            if (data.pilote !== undefined) afficherPilote(data.pilote);
        }

        function reinitialiserAffichageLiaisons() {
            dotManetteEsp32.className = 'status-dot';
            labelManetteEsp32.textContent = 'Inconnue (robot non connecté)';
            dotGps.className = 'status-dot';
            labelGpsLiaison.textContent = 'Inconnu (robot non connecté)';
            afficherPilote(null);
        }

        // ==================== GPS ====================
        const ETATS_GPS = {
            'fix':        { badge: 'Position acquise', couleur: '#4CAF50', dot: 'ok',
                            note: 'Position reçue du module GPS, transmise par le WiFi du robot.' },
            'simulation': { badge: 'Simulation', couleur: '#9C27B0', dot: 'ok',
                            note: 'Position SIMULÉE par le robot (GPS_SIMULATION = 1 dans robot.ino). Toute la chaîne ESP32 → WiFi → IHM est testée, sans le module.' },
            'recherche':  { badge: 'Recherche satellites...', couleur: '#FF9800', dot: 'warn',
                            note: 'Le module GPS est bien câblé et communique avec l’ESP32 (données reçues). Il lui faut maintenant une vue dégagée sur le ciel pour calculer sa position : dehors ou contre une fenêtre, compter 1 à 5 minutes au premier démarrage.' },
            'absent':     { badge: 'Module non détecté', couleur: '#f44336', dot: 'error',
                            note: 'Le robot ne reçoit AUCUNE donnée du GPS. Vérifier le câblage : TX du GPS → D7 (GPIO44) de l’ESP32 robot, VCC → 3V3, GND → GND.' },
            'illisible':  { badge: 'Données illisibles', couleur: '#f44336', dot: 'error',
                            note: 'Le robot reçoit des données du GPS mais ne peut pas les décoder : vérifier la vitesse série (9600 bauds par défaut pour l’Air530) et le fil de masse commun.' },
        };

        let gpsOrigine = null, gpsChemin = [], gpsDernier = null, gpsDistance = 0, gpsCapRad = null;
        let carteDernierePos = null;
        const MAX_POINTS_GPS = 2000;

        function metresDepuisOrigine(lat, lon) {
            return { x: (lon - gpsOrigine.lon) * 111320 * Math.cos(gpsOrigine.lat * Math.PI / 180), y: (lat - gpsOrigine.lat) * 110540 };
        }

        function afficherGPS(data) {
            // Compatibilité avec un ancien firmware qui n'envoie que gpsFix
            const etat = data.gpsEtat || (data.gpsFix ? 'fix' : 'recherche');
            const infos = ETATS_GPS[etat] || ETATS_GPS['recherche'];
            const fix = !!data.gpsFix;
            const lat = Number(data.gpsLat) || 0, lon = Number(data.gpsLon) || 0;
            const connue = fix || !!data.gpsDejaFixe;
            const sats = Number(data.gpsSatellites) || 0;
            const satsVisibles = data.gpsSatellitesVisibles !== undefined ? Number(data.gpsSatellitesVisibles) || 0 : null;

            let badge = infos.badge;
            if (etat === 'recherche' && satsVisibles) badge = `Recherche... (${satsVisibles} satellites entendus)`;
            document.querySelectorAll('[data-gps=badge]').forEach(el => { el.textContent = badge; el.style.background = infos.couleur; el.style.color = '#fff'; });
            document.querySelectorAll('[data-gps=lat]').forEach(el => { el.textContent = connue ? lat.toFixed(6) : '--'; el.classList.toggle('perime', !fix); });
            document.querySelectorAll('[data-gps=lon]').forEach(el => { el.textContent = connue ? lon.toFixed(6) : '--'; el.classList.toggle('perime', !fix); });
            document.querySelectorAll('[data-gps=sat]').forEach(el => el.textContent = satsVisibles !== null ? `${sats} / ${satsVisibles}` : sats);

            dotGps.className = 'status-dot ' + infos.dot;
            labelGpsLiaison.textContent = fix ? `${infos.badge} : ${lat.toFixed(5)}, ${lon.toFixed(5)}` : badge;

            let note = infos.note;
            if (!fix && connue) note += ` Dernière position connue affichée en gris.`;
            document.getElementById('note-gps').textContent = note;
            document.getElementById('val-gps-module').textContent = infos.badge;
            document.getElementById('val-gps-vitesse').textContent = fix && data.gpsVitesseKmh !== undefined ? Number(data.gpsVitesseKmh).toFixed(1) : '--';
            document.getElementById('val-gps-altitude').textContent = fix && data.gpsAltitude !== undefined ? Number(data.gpsAltitude).toFixed(1) : '--';
            document.getElementById('val-gps-hdop').textContent = fix && data.gpsHdop !== undefined ? `${Number(data.gpsHdop).toFixed(2)} (plus c'est bas, mieux c'est)` : '—';
            document.getElementById('val-gps-cap').textContent = fix && data.gpsCap !== undefined ? `${Number(data.gpsCap).toFixed(0)}°` : '—';
            document.getElementById('val-gps-heure').textContent = data.gpsHeureUTC || '—';
            document.getElementById('val-gps-caracteres').textContent = data.gpsCaracteres ?? '—';
            document.getElementById('val-gps-trames').textContent = data.gpsTramesOk !== undefined ? `${data.gpsTramesOk} / ${data.gpsTramesKo}` : '—';

            if (connue) {
                document.getElementById('lien-osm').href = `https://www.openstreetmap.org/?mlat=${lat}&mlon=${lon}#map=19/${lat}/${lon}`;
                document.getElementById('lien-gmaps').href = `https://www.google.com/maps?q=${lat},${lon}`;
            }
            if (fix) {
                ajouterPointGPS(lat, lon, Number(data.gpsVitesseKmh) || 0, Number(data.gpsCap) || 0);
                mettreAJourCarte(lat, lon);
            }
        }

        function afficherGPSDeconnecte() {
            document.querySelectorAll('[data-gps=badge]').forEach(el => { el.textContent = 'Robot non connecté'; el.style.background = '#666'; });
            document.querySelectorAll('[data-gps=lat],[data-gps=lon]').forEach(el => el.classList.add('perime'));
            document.getElementById('note-gps').textContent = 'Connectez-vous au robot pour recevoir la position GPS.';
        }

        function ajouterPointGPS(lat, lon, vitesseKmh, capDeg) {
            if (!gpsOrigine) gpsOrigine = { lat, lon };
            const p = metresDepuisOrigine(lat, lon);
            if (!gpsDernier || Math.hypot(p.x - gpsDernier.x, p.y - gpsDernier.y) > 1.0) { // filtre le bruit GPS (~1 m)
                if (gpsDernier) gpsDistance += Math.hypot(p.x - gpsDernier.x, p.y - gpsDernier.y);
                gpsChemin.push(p); if (gpsChemin.length > MAX_POINTS_GPS) gpsChemin.shift();
                gpsDernier = p;
            }
            gpsCapRad = vitesseKmh > 1 ? (90 - capDeg) * Math.PI / 180 : null; // cap GPS : 0° = nord, sens horaire
            dessinerTraceGPS();
        }

        function reinitialiserTraceGPS() { gpsOrigine = null; gpsChemin = []; gpsDernier = null; gpsDistance = 0; gpsCapRad = null; dessinerTraceGPS(); }
        document.getElementById('btn-reset-trace-gps').addEventListener('click', reinitialiserTraceGPS);

        const chkCarte = document.getElementById('chk-carte'), gpsCarte = document.getElementById('gps-carte');
        chkCarte.addEventListener('change', () => { gpsCarte.classList.toggle('visible', chkCarte.checked); carteDernierePos = null; });
        function mettreAJourCarte(lat, lon) {
            if (!chkCarte.checked) return;
            // On ne recharge la carte que si le robot a bougé de plus de ~10 m
            if (carteDernierePos && Math.hypot((lat - carteDernierePos.lat) * 110540, (lon - carteDernierePos.lon) * 111320 * Math.cos(lat * Math.PI / 180)) < 10) return;
            carteDernierePos = { lat, lon };
            const d = 0.0015;
            gpsCarte.src = `https://www.openstreetmap.org/export/embed.html?bbox=${lon - d},${lat - d},${lon + d},${lat + d}&layer=mapnik&marker=${lat},${lon}`;
        }

        // ==================== COMMUNICATION HTTP ====================
        function setEsp32Status(state, text) { dotEsp32.className = 'status-dot' + (state ? ' '+state : ''); labelEsp32Status.textContent = text; }
        function setMode(connecte) { modeLabel.textContent = connecte ? 'Robot réel (WiFi)' : 'Démo (simulation locale)'; modeLabel.style.color = connecte ? '#4CAF50' : '#FF9800'; }

        async function fetchAvecTimeout(url) {
            const ctrl = new AbortController(); const t = setTimeout(()=>ctrl.abort(), HTTP_TIMEOUT_MS);
            try { return await fetch(url, {signal:ctrl.signal, cache:'no-store'}); } finally { clearTimeout(t); }
        }

        function majEtatLiaison(ok) {
            if (!esp32Url) return;
            if (ok) {
                derniereReponseOkMs = performance.now();
                if (!httpConnecte) {
                    httpConnecte = true; dernierTelemetryTimestamp = null;
                    setEsp32Status('ok', `Connecté (${esp32Url.replace('http://','')})`);
                    btnConnect.textContent = 'Déconnecter'; btnConnect.classList.add('connected'); setMode(true);
                }
            } else if (httpConnecte) {
                if (performance.now() - derniereReponseOkMs > HTTP_PERTE_LIAISON_MS) {
                    httpConnecte = false; setEsp32Status('warn', 'Liaison perdue, nouvelles tentatives en cours...'); setMode(false);
                    reinitialiserAffichageLiaisons(); afficherGPSDeconnecte();
                }
            } else if (performance.now() - tentativeConnexionDepuisMs > HTTP_PERTE_LIAISON_MS) {
                setEsp32Status('error', "Injoignable — ce PC est-il connecté au WiFi ROBOT_ESP32 ? (nouvelles tentatives en cours)");
            }
        }

        async function envoyerCommandeHttp(pwrRoueG, pwrRoueD) {
            if (!esp32Url || cmdEnCours) return;
            const cle = `${pwrRoueG},${pwrRoueD}`, maintenant = performance.now();
            // Au repos, inutile d'inonder le robot : il coupe de lui-même sans commande fraîche.
            if (cle === derniereCmdEnvoyee && pwrRoueG === 0 && pwrRoueD === 0 && maintenant - derniereCmdEnvoyeeMs < INTERVALLE_CMD_REPOS_MS) return;
            derniereCmdEnvoyee = cle; derniereCmdEnvoyeeMs = maintenant;
            cmdEnCours = true;
            try { const r = await fetchAvecTimeout(`${esp32Url}/cmd?src=ihm&rg=${pwrRoueG}&rd=${pwrRoueD}`); majEtatLiaison(r.ok); }
            catch(e) { majEtatLiaison(false); } finally { cmdEnCours = false; }
        }

        function envoyerArretUrgence() {
            manuelRoueAvance = 0; manuelRoueTourne = 0;
            if (!esp32Url) return;
            fetchAvecTimeout(`${esp32Url}/cmd?src=ihm&rg=0&rd=0&stop=1`).catch(() => {});
        }

        async function lireTelemetrieHttp() {
            if (!esp32Url || telemetrieEnCours) return;
            telemetrieEnCours = true;
            try {
                const r = await fetchAvecTimeout(`${esp32Url}/telemetrie`);
                if (!r.ok) { majEtatLiaison(false); return; }
                const data = await r.json();
                majEtatLiaison(true);
                traiterTelemetrieRecue(data);
            } catch(e) { majEtatLiaison(false); } finally { telemetrieEnCours = false; }
        }

        function traiterTelemetrieRecue(data) {
            try {
                const vG = Number(data.vitesseRoueG)||0, vD = Number(data.vitesseRoueD)||0;
                const maintenant = performance.now();
                let deltaT = 0;
                if (dernierTelemetryTimestamp !== null) { const d=(maintenant-dernierTelemetryTimestamp)/1000; if (d>0&&d<2) deltaT=d; }
                dernierTelemetryTimestamp = maintenant;
                afficherTelemetrie(vG, vD, Number(data.distance)||0, Number(data.batterie)||0, Number(data.soc)||0, data.etatBMS||'OK', Number(data.courant)||0, Number(data.consommation)||0, deltaT);
                if (deltaT>0) mettreAJourPosition(vG, vD, deltaT);
                if (data.joystickX !== undefined) {
                    afficherJoystickPhysique(Number(data.joystickX)||0, Number(data.joystickY)||0, !!data.joystickBouton, data.manetteConnectee !== false);
                }
                if (data.gpsLat !== undefined) afficherGPS(data);
                afficherLiaisons(data);
            } catch(e) { console.warn('Trame invalide:', data, e); }
        }

        function connecterEsp32(ip) {
            esp32Url = `http://${ip}`;
            try { localStorage.setItem('ipRobot', ip); } catch (e) {}
            derniereReponseOkMs = performance.now(); tentativeConnexionDepuisMs = performance.now();
            derniereCmdEnvoyee = null;
            setEsp32Status('warn', 'Connexion en cours...');
            btnConnect.textContent = 'Annuler';
            telemetrieTimer = setInterval(lireTelemetrieHttp, INTERVALLE_TELEMETRIE_MS);
            lireTelemetrieHttp();
        }

        function deconnecterEsp32() {
            esp32Url = null; httpConnecte = false; clearInterval(telemetrieTimer); telemetrieTimer = null;
            setEsp32Status('', 'Déconnecté'); btnConnect.textContent = 'Se connecter'; btnConnect.classList.remove('connected'); setMode(false);
            reinitialiserAffichageLiaisons(); afficherGPSDeconnecte();
        }

        btnConnect.addEventListener('click', () => {
            if (esp32Url) { deconnecterEsp32(); return; }
            const ip = ipInput.value.trim().replace(/^https?:\/\//, '').replace(/\/+$/, '');
            if (!ip) { alert("Merci de saisir l'adresse IP de l'ESP32 (ex : 192.168.4.1)."); return; }
            connecterEsp32(ip);
        });

        // ==================== AFFICHAGE TÉLÉMÉTRIE ====================
        function afficherTelemetrie(vG, vD, distance, batterie, soc, etat, courant, consommation, deltaT) {
            const vitesseGlobale = (vG+vD)/2;
            if (deltaT>0) { distanceCumuleeRoueG += Math.abs(vG)*deltaT; distanceCumuleeRoueD += Math.abs(vD)*deltaT; }

            document.getElementById('val-vitesse-globale').textContent = vitesseGlobale.toFixed(2);
            document.getElementById('val-distance').textContent = distance.toFixed(2);
            document.getElementById('val-batterie').textContent = batterie.toFixed(2);
            document.getElementById('val-batterie-2').textContent = batterie.toFixed(2);
            document.getElementById('val-soc').textContent = Math.round(soc);
            document.getElementById('val-soc-2').textContent = Math.round(soc);
            document.getElementById('val-courant').textContent = courant.toFixed(2);
            document.getElementById('val-consommation').textContent = consommation.toFixed(3);
            document.getElementById('val-vitesse-roue-g').textContent = vG.toFixed(2);
            document.getElementById('val-vitesse-roue-d').textContent = vD.toFixed(2);
            document.getElementById('val-distance-roue-g').textContent = distanceCumuleeRoueG.toFixed(2);
            document.getElementById('val-distance-roue-d').textContent = distanceCumuleeRoueD.toFixed(2);

            const badge = document.getElementById('badge-etat-bms');
            badge.textContent = etat; badge.className = 'badge-etat-bms ' + (etat==='OK' ? 'ok' : 'alerte');
            const alerte = document.getElementById('bms-alerte');
            if (etat !== 'OK') {
                alerte.classList.add('visible');
                const messages = {'SOUS-TENSION':'Sous-tension du pack — décharge à couper.', 'SURTENSION':'Surtension du pack — charge à couper.', 'SURINTENSITE':'Courant excessif détecté.'};
                document.getElementById('bms-alerte-texte').textContent = messages[etat] || 'Protection déclenchée';
            } else { alerte.classList.remove('visible'); }

            globalSpeedChart.push(vitesseGlobale); consumptionChart.push(consommation);
            batteryChart.push(batterie); socChart.push(soc); currentChart.push(courant);
            speedChartRoueG.push(vG); distanceChartRoueG.push(distanceCumuleeRoueG);
            speedChartRoueD.push(vD); distanceChartRoueD.push(distanceCumuleeRoueD);
        }

        // ==================== TRACÉS (odométrie + GPS) ====================
        function pasDeGrille(etendue) {
            const pas = [1, 2, 5, 10, 20, 50, 100, 200, 500, 1000];
            return pas.find(p => etendue / p <= 12) || 2000;
        }

        // Dessine un chemin (coordonnées en mètres) centré et mis à l'échelle dans le canvas.
        function dessinerChemin(canvas, points, courant, headingRad, couleur) {
            const ctx = canvas.getContext('2d'), w = canvas.width, h = canvas.height;
            ctx.fillStyle = '#14141f'; ctx.fillRect(0, 0, w, h);
            const marge = 0.5;
            const xs = points.map(p => p.x).concat([0, courant.x]), ys = points.map(p => p.y).concat([0, courant.y]);
            const minX = Math.min(...xs) - marge, maxX = Math.max(...xs) + marge;
            const minY = Math.min(...ys) - marge, maxY = Math.max(...ys) + marge;
            const rangeX = Math.max(maxX - minX, 1), rangeY = Math.max(maxY - minY, 1), scale = Math.min(w / rangeX, h / rangeY);
            const cx = (minX + maxX) / 2, cy = (minY + maxY) / 2;
            const toScreen = (x, y) => ({ sx: w / 2 + (x - cx) * scale, sy: h / 2 - (y - cy) * scale });
            const pas = pasDeGrille(Math.max(w, h) / scale);
            const x0 = cx - w / 2 / scale, x1 = cx + w / 2 / scale, y0 = cy - h / 2 / scale, y1 = cy + h / 2 / scale;
            ctx.strokeStyle = '#2a2a40'; ctx.lineWidth = 1;
            for (let gx = Math.ceil(x0 / pas) * pas; gx <= x1; gx += pas) { const p1 = toScreen(gx, y0), p2 = toScreen(gx, y1); ctx.beginPath(); ctx.moveTo(p1.sx, p1.sy); ctx.lineTo(p2.sx, p2.sy); ctx.stroke(); }
            for (let gy = Math.ceil(y0 / pas) * pas; gy <= y1; gy += pas) { const p1 = toScreen(x0, gy), p2 = toScreen(x1, gy); ctx.beginPath(); ctx.moveTo(p1.sx, p1.sy); ctx.lineTo(p2.sx, p2.sy); ctx.stroke(); }
            ctx.strokeStyle = couleur; ctx.lineWidth = 2; ctx.beginPath();
            points.forEach((p, i) => { const { sx, sy } = toScreen(p.x, p.y); if (i === 0) ctx.moveTo(sx, sy); else ctx.lineTo(sx, sy); }); ctx.stroke();
            const depart = toScreen(0, 0); ctx.fillStyle = '#2196F3'; ctx.beginPath(); ctx.arc(depart.sx, depart.sy, 4, 0, 2 * Math.PI); ctx.fill();
            const pos = toScreen(courant.x, courant.y); ctx.fillStyle = '#FF9800';
            if (headingRad === null) { ctx.beginPath(); ctx.arc(pos.sx, pos.sy, 6, 0, 2 * Math.PI); ctx.fill(); }
            else { ctx.save(); ctx.translate(pos.sx, pos.sy); ctx.rotate(-headingRad); ctx.beginPath(); ctx.moveTo(10, 0); ctx.lineTo(-6, 6); ctx.lineTo(-6, -6); ctx.closePath(); ctx.fill(); ctx.restore(); }
            ctx.fillStyle = '#777'; ctx.font = '12px sans-serif'; ctx.textAlign = 'left'; ctx.textBaseline = 'bottom';
            ctx.fillText(`grille : ${pas} m`, 8, h - 6);
        }

        const traceCanvas = document.getElementById('traceCanvas');
        const traceScaleLabel = document.getElementById('trace-scale-label');
        function mettreAJourPosition(vG, vD, deltaT) {
            if (!isFinite(deltaT)||deltaT<=0) return;
            const v=(vG+vD)/2, omega=(vD-vG)/ENTRAXE_M;
            posX += v*Math.cos(heading)*deltaT; posY += v*Math.sin(heading)*deltaT; heading += omega*deltaT;
            const dernier = chemin[chemin.length-1];
            if (!dernier || Math.hypot(posX-dernier.x, posY-dernier.y)>0.01) { chemin.push({x:posX,y:posY}); if (chemin.length>MAX_POINTS_TRACE) chemin.shift(); }
            dessinerTrace();
        }
        function reinitialiserTrace() { posX=0;posY=0;heading=0;chemin=[{x:0,y:0}];dernierTelemetryTimestamp=null;dessinerTrace(); }
        document.getElementById('btn-reset-trace').addEventListener('click', reinitialiserTrace);
        function dessinerTrace() {
            if (traceCanvas.offsetParent === null) return; // onglet caché
            dessinerChemin(traceCanvas, chemin, { x: posX, y: posY }, heading, '#4CAF50');
            traceScaleLabel.textContent = `Position : (${posX.toFixed(2)}, ${posY.toFixed(2)}) m — cap ${((heading*180/Math.PI)%360).toFixed(0)}°`;
        }

        const traceGpsCanvas = document.getElementById('traceGpsCanvas');
        const traceGpsLabel = document.getElementById('trace-gps-label');
        function dessinerTraceGPS() {
            if (traceGpsCanvas.offsetParent === null) return;
            if (!gpsOrigine) {
                dessinerChemin(traceGpsCanvas, [], { x: 0, y: 0 }, null, '#8BC34A');
                traceGpsLabel.textContent = 'En attente d’une position GPS...';
                return;
            }
            dessinerChemin(traceGpsCanvas, gpsChemin, gpsDernier, gpsCapRad, '#8BC34A');
            traceGpsLabel.textContent = `Départ : ${gpsOrigine.lat.toFixed(6)}, ${gpsOrigine.lon.toFixed(6)} — écart actuel : ${gpsDernier.x.toFixed(1)} m (est), ${gpsDernier.y.toFixed(1)} m (nord) — distance parcourue : ${gpsDistance.toFixed(1)} m`;
        }

        // ==================== BOUTONS SECOURS ====================
        const boutonsRoues = { 'cmd-roue-avant':[1,0], 'cmd-roue-arriere':[-1,0], 'cmd-roue-gauche':[0,-1], 'cmd-roue-droite':[0,1] };
        for (const [id,[a,t]] of Object.entries(boutonsRoues)) {
            const btn = document.getElementById(id);
            const activer = () => { manuelRoueAvance=a; manuelRoueTourne=t; };
            const relacher = () => { manuelRoueAvance=0; manuelRoueTourne=0; };
            btn.addEventListener('mousedown', activer); btn.addEventListener('mouseup', relacher); btn.addEventListener('mouseleave', relacher);
            btn.addEventListener('touchstart', e=>{e.preventDefault(); activer();}); btn.addEventListener('touchend', e=>{e.preventDefault(); relacher();});
        }
        document.getElementById('cmd-roue-stop').addEventListener('click', envoyerArretUrgence);
        document.getElementById('cmd-stop-global').addEventListener('click', envoyerArretUrgence);

        // ==================== MANETTE USB (PC) ====================
        window.addEventListener('gamepadconnected', e => { gamepadIndex=e.gamepad.index; dotGamepad.className='status-dot ok'; labelGamepadStatus.textContent=`Connectée : ${e.gamepad.id}`; });
        window.addEventListener('gamepaddisconnected', e => {
            if (gamepadIndex===e.gamepad.index) { gamepadIndex=null; gamepadRoueAvance=0; gamepadRoueTourne=0; gamepadCroixRoueAvance=0; gamepadCroixRoueTourne=0;
                dotGamepad.className='status-dot'; labelGamepadStatus.textContent='Aucune manette détectée'; stickDotRoues.style.left='50%'; stickDotRoues.style.top='50%'; }
        });

        let boutonBPrecedent = false;
        function lireManette() {
            if (gamepadIndex !== null) {
                const gp = navigator.getGamepads()[gamepadIndex];
                if (gp) {
                    const ax = gp.axes[0]||0, ay = gp.axes[1]||0;
                    stickDotRoues.style.left = `${50+Math.max(-1,Math.min(1,ax))*40}%`;
                    stickDotRoues.style.top = `${50+Math.max(-1,Math.min(1,ay))*40}%`;
                    let avance=0, tourne=0;
                    if (Math.abs(ay)>DEADZONE) avance=-ay;
                    if (Math.abs(ax)>DEADZONE) tourne=ax;
                    gamepadRoueAvance=avance; gamepadRoueTourne=tourne;

                    let croixA=0, croixT=0;
                    if (avance===0 && tourne===0 && gp.buttons.length>15) {
                        if (gp.buttons[12]?.pressed) croixA=1; else if (gp.buttons[13]?.pressed) croixA=-1;
                        if (gp.buttons[14]?.pressed) croixT=-1; else if (gp.buttons[15]?.pressed) croixT=1;
                    }
                    gamepadCroixRoueAvance=croixA; gamepadCroixRoueTourne=croixT;

                    const boutonB = !!gp.buttons[1]?.pressed;
                    if (boutonB) { gamepadRoueAvance=0; gamepadRoueTourne=0; gamepadCroixRoueAvance=0; gamepadCroixRoueTourne=0; }
                    if (boutonB && !boutonBPrecedent) envoyerArretUrgence();
                    boutonBPrecedent = boutonB;
                }
            }
            requestAnimationFrame(lireManette);
        }
        requestAnimationFrame(lireManette);

        // ==================== BOUCLE PRINCIPALE ====================
        setInterval(() => {
            const puissanceMax = parseInt(sliderSpeed.value);
            let avance, tourne;
            if (gamepadRoueAvance!==0 || gamepadRoueTourne!==0) { avance=gamepadRoueAvance; tourne=gamepadRoueTourne; }
            else if (gamepadCroixRoueAvance!==0 || gamepadCroixRoueTourne!==0) { avance=gamepadCroixRoueAvance; tourne=gamepadCroixRoueTourne; }
            else { avance=manuelRoueAvance; tourne=manuelRoueTourne; }
            const pwrRoueG = Math.round(Math.max(-1,Math.min(1, avance+tourne))*puissanceMax);
            const pwrRoueD = Math.round(Math.max(-1,Math.min(1, avance-tourne))*puissanceMax);

            jsonOut.textContent = `/cmd?src=ihm&rg=${pwrRoueG}&rd=${pwrRoueD}`;

            if (esp32Url) { envoyerCommandeHttp(pwrRoueG, pwrRoueD); if (httpConnecte) return; }

            // ---- Mode démo ----
            const chargeMecanique = parseInt(sliderSlope.value);
            const inertie = 0.08;
            const appliquerInertie = (act,cib) => { if (etatBMS!=='OK') return 0; if (Math.abs(act-cib)<inertie) return cib; return act<cib ? act+inertie : act-inertie; };
            const vitesseCibleG = (pwrRoueG/100)*VITESSE_MAX_ROUE, vitesseCibleD = (pwrRoueD/100)*VITESSE_MAX_ROUE;
            vitesseActuelleRoueG = appliquerInertie(vitesseActuelleRoueG, vitesseCibleG);
            vitesseActuelleRoueD = appliquerInertie(vitesseActuelleRoueD, vitesseCibleD);

            let c = 0.1;
            if (Math.abs(vitesseActuelleRoueG)>0.05 || Math.abs(vitesseActuelleRoueD)>0.05) {
                c += (Math.abs(vitesseActuelleRoueG)+Math.abs(vitesseActuelleRoueD))*0.6 + (chargeMecanique/100)*3.5;
            }
            courantPack = c;

            const vitesseLineaire = (vitesseActuelleRoueG+vitesseActuelleRoueD)/2;
            distanceTotale += Math.abs(vitesseLineaire)*dt;
            consommationTotale += courantPack*dt/3600;

            if (etatBMS !== 'SOUS-TENSION') { tensionPack -= (courantPack*dt)/300; if (tensionPack<8.5) tensionPack=8.5; }
            socPack = socDepuisTension(tensionPack/3);

            if (tensionPack < SEUIL_SOUS_TENSION_PACK) etatBMS='SOUS-TENSION';
            else if (tensionPack > SEUIL_SURTENSION_PACK) etatBMS='SURTENSION';
            else if (courantPack > SEUIL_SURINTENSITE) etatBMS='SURINTENSITE';
            else etatBMS='OK';

            afficherTelemetrie(vitesseActuelleRoueG, vitesseActuelleRoueD, distanceTotale, tensionPack, socPack, etatBMS, courantPack, consommationTotale, dt);
            mettreAJourPosition(vitesseActuelleRoueG, vitesseActuelleRoueD, dt);
        }, intervalleUpdateMs);

        // ==================== DÉMARRAGE ====================
        // Si la page est servie par le robot (http://192.168.4.1), on se connecte tout seul.
        window.addEventListener('load', () => {
            switchTab('tab-global');
            dessinerTrace();
            const serviParLeRobot = location.protocol === 'http:' && location.host !== '' && !['localhost', '127.0.0.1'].includes(location.hostname);
            let ipMemorisee = null;
            try { ipMemorisee = localStorage.getItem('ipRobot'); } catch (e) {}
            if (serviParLeRobot) { ipInput.value = location.host; connecterEsp32(location.host); }
            else { ipInput.value = ipMemorisee || '192.168.4.1'; }
        });
    </script>
</body>
</html>
)IHM_HTML_FIN";
