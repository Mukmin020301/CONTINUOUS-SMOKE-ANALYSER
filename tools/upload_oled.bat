@echo off
cd /d "%~dp0.."
echo ===================================================
echo  FAT SMOKE 1.0 - OLED DASHBOARD FLASHER
echo ===================================================
echo.
echo Compiling sketch for Arduino Nano...
.\arduino_tools\arduino-cli.exe --config-file tools\arduino_cli.yaml compile --fqbn arduino:avr:nano .\firmware\fat_smoke_oled_meter
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed!
    pause
    exit /b %errorlevel%
)

echo.
echo Uploading to Arduino Nano on COM5 (CH340 Old Bootloader)...
.\arduino_tools\arduino-cli.exe --config-file tools\arduino_cli.yaml upload -p COM5 --fqbn arduino:avr:nano:cpu=atmega328old .\firmware\fat_smoke_oled_meter
if %errorlevel% neq 0 (
    echo Trying standard bootloader...
    .\arduino_tools\arduino-cli.exe --config-file tools\arduino_cli.yaml upload -p COM5 --fqbn arduino:avr:nano .\firmware\fat_smoke_oled_meter
)

echo.
echo ===================================================
echo  UPLOAD COMPLETE! Check your OLED screen!
echo ===================================================
pause
