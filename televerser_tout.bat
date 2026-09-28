@echo off
rem Double-clic : compile et televerse sur le bon ESP32 (voir tools\televerser.py)
chcp 65001 >nul
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel%==0 (py -3 tools\televerser.py tout %*) else (python tools\televerser.py tout %*)
pause
