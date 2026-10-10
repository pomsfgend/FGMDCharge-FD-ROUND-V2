@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
if not exist "%IDF_DIR%\export.bat" (echo ERROR: ESP-IDF 5.0.9 is not installed. Run BUILD_RELEASE.bat first.&pause&exit /b 1)
call "%IDF_DIR%\export.bat"
if errorlevel 1 (echo ERROR: Could not activate ESP-IDF environment.&pause&exit /b 1)
set "PORT="
set "BACKUP_DIR=%~dp0backups"
set "APP=%~dp0release\FGMDCharge_FD_ROUND_V2.bin"

if not exist "%APP%" (
  echo ERROR: firmware is missing: %APP%
  echo Run BUILD_RELEASE.bat first.
  pause
  exit /b 1
)

where python >nul 2>&1
if errorlevel 1 (
  echo ERROR: python not found. Run BUILD_RELEASE.bat once first.
  pause
  exit /b 1
)
python -m esptool version >nul 2>&1
if errorlevel 1 (
  echo ERROR: esptool is not installed.
  echo Run BUILD_RELEASE.bat once first.
  pause
  exit /b 1
)

if not exist "%BACKUP_DIR%" mkdir "%BACKUP_DIR%"
if not defined CUBE_PORT set /p CUBE_PORT=Enter cube COM port (example COM7):
set "PORT=%CUBE_PORT%"
if "%PORT%"=="" (
  echo ERROR: COM port is empty.
  pause
  exit /b 1
)

if exist "%BACKUP_DIR%\cube_before_v2.bin" copy /y "%BACKUP_DIR%\cube_before_v2.bin" "%BACKUP_DIR%\cube_before_v2.previous.bin" >nul

echo.
echo STEP 1/3 — FULL 4 MiB BACKUP
echo Reading flash from %PORT%...
python -m esptool --chip esp32c3 --port "%PORT%" read_flash 0x0 0x400000 "%BACKUP_DIR%\cube_before_v2.bin"
if errorlevel 1 goto FAIL
certutil -hashfile "%BACKUP_DIR%\cube_before_v2.bin" SHA256

echo.
echo STEP 2/3 — VERIFY NEW APPLICATION
python tools\check_app_image.py "%APP%"
if errorlevel 1 goto FAIL

echo.
choice /C YN /N /M "Write NEW APP ONLY to OTA1 at 0x1A0000? [Y/N] "
if errorlevel 2 (
  echo Cancelled. Backup remains saved.
  pause
  exit /b 0
)

python -m esptool --chip esp32c3 --port "%PORT%" write_flash 0x1A0000 "%APP%"
if errorlevel 1 goto FAIL

echo.
echo STEP 3/3 — OTA SLOT
python tools\ota_slot.py --port "%PORT%" show
if errorlevel 1 goto FAIL
choice /C YN /N /M "Switch boot selection to OTA1 now? [Y/N] "
if errorlevel 2 (
  echo OTA1 was written but boot selection was NOT changed.
  echo To select it later, use:
  echo python tools\ota_slot.py --port "%PORT%" ota1
  pause
  exit /b 0
)
python tools\ota_slot.py --port "%PORT%" ota1
if errorlevel 1 goto FAIL

echo.
echo ================================================================
echo OTA1 UPDATED AND SELECTED
echo Old OTA0 and the rest of the flash were not overwritten.
echo Reboot/reset the cube and run 03_MONITOR.bat if diagnostics are needed.
echo ================================================================
echo Wi-Fi: FGMDCharge-V2 / FDcube2026
echo Web:   http://192.168.4.1/
echo BLE:   FGMDCharge
echo.
pause
exit /b 0

:FAIL
echo.
echo FLASH FAILED. Keep the saved backup.
pause
exit /b 1
