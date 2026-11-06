@echo off
cd /d "%~dp0.."
setlocal enabledelayedexpansion
echo ===================================================
echo  FAT SMOKE 1.0 - PIC18F46K22 XC8 COMPILER
echo ===================================================
echo.
set XC8="D:\Program Files\Microchip\xc8\v3.00\bin\xc8-cc.exe"

if not exist %XC8% (
    set XC8="C:\Program Files\Microchip\xc8\v2.46\bin\xc8-cc.exe"
)

echo Compiling firmware/legacy/main.c for PIC18F46K22...
%XC8% -mcpu=18F46K22 -o fat_smoke_firmware.hex firmware\legacy\main.c

if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed!
    pause
    exit /b %errorlevel%
)

:: Get current timestamp (YYYYMMDD_HHMM)
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
set TIMESTAMP=%datetime:~0,8%_%datetime:~8,4%
set TS_HEX=fat_smoke_firmware_%TIMESTAMP%.hex

copy /y fat_smoke_firmware.hex %TS_HEX% >nul
copy /y fat_smoke_firmware.hex fat_smoke_firmware_REV5.hex >nul

echo.
echo [SUCCESS] Firmware compiled successfully!
echo   [1] Standard:    fat_smoke_firmware.hex
echo   [2] Explicit:    fat_smoke_firmware_REV5.hex (USE THIS IN PICKIT 2!)
echo   [3] Timestamped: %TS_HEX%
echo.
