@echo off
title Push Continuous Smoke Analyser to GitHub
cd /d "%~dp0"
echo ========================================================
echo   Pushing Continuous Smoke Analyser to GitHub...
echo ========================================================
git push -u origin main
echo.
echo ========================================================
echo   Selesai! Sila tekan apa-apa kekunci untuk tutup.
echo ========================================================
pause
