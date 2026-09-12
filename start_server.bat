@echo off
title ESP32 OTA Management Web Server
cd /d "%~dp0server"
echo ======================================================
echo    MEMULAI ESP32 OTA MANAGEMENT WEB SERVER
echo ======================================================
echo.
node server.js
pause
