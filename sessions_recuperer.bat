@echo off
rem Double-clic : recupere les sessions du robot dans la base de donnees (sessions\sessions.db) et affiche la liste
chcp 65001 >nul
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel%==0 (py -3 tools\sessions.py %*) else (python tools\sessions.py %*)
pause
