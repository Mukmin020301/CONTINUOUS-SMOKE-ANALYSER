@echo off
echo ===================================================
echo  FAT SMOKE 1.0 - PIC18F46K22 FIRMWARE FLASHER
echo ===================================================
echo.
set IPECMD="D:\Program Files\Microchip\MPLABX\v6.20\mplab_platform\mplab_ipe\ipecmd.exe"
set HEX="D:\Users\ANG\Desktop\Mukmin\CONTINUOUS SMOKE ANALYSER\fat_smoke_firmware.hex"

if not exist %IPECMD% (
    echo [ERROR] ipecmd.exe not found! Please flash fat_smoke_firmware.hex manually via MPLAB IPE GUI.
    pause
    exit /b 1
)

echo Flashing PIC18F46K22 via PICkit 3 / 4 / Snap...
echo (Ensure board 12V is ON and PICkit is connected to P2 header)
echo.

%IPECMD% -P18F46K22 -TPPK3 -M -F%HEX%
if %errorlevel% neq 0 (
    echo Trying PICkit 4 / Snap...
    %IPECMD% -P18F46K22 -TPPK4 -M -F%HEX%
)
if %errorlevel% neq 0 (
    %IPECMD% -P18F46K22 -TPSNAP -M -F%HEX%
)

echo.
echo ===================================================
echo  FLASHING OPERATION COMPLETE!
echo ===================================================
pause
