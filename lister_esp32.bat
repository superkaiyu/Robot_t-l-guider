@echo off
rem Double-clic : liste les ESP32 branches et indique lequel est le Robot / la Manette
chcp 65001 >nul
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel%==0 (py -3 tools\televerser.py liste) else (python tools\televerser.py liste)
pause
