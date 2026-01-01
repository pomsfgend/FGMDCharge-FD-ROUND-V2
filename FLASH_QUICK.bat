@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
if not exist "%IDF_DIR%\export.bat" (echo ERROR: IDF missing&pause&exit /b 1)
call "%IDF_DIR%\export.bat"
if errorlevel 1 (pause&exit /b 1)

set "APP=%~dp0release\FGMDCharge_FD_ROUND_V2.bin"
if not exist "%APP%" (
  echo ERROR: run START_BUILD.cmd or QUICK_BUILD.bat first
  pause
  exit /b 1
)

set /p PORT=Enter COM port (e.g. COM14): 
if "%PORT%"=="" (echo empty port&pause&exit /b 1)

echo.
echo [1/2] Fast OTA1 app flash @ 921600
python -m esptool --chip esp32c3 --port "%PORT%" -b 921600 --before default_reset --after hard_reset write_flash 0x1A0000 "%APP%"
if errorlevel 1 (echo FLASH FAILED&pause&exit /b 1)

echo Waiting for USB serial after reset...
timeout /t 2 /nobreak >nul

echo [2/2] Force boot slot OTA1 (otadata)
python tools\ota_slot.py --port "%PORT%" ota1
if errorlevel 1 (
  echo WARNING: otadata write failed. Power-cycle the cube, then run:
  echo   python tools\ota_slot.py --port %PORT% ota1
  pause
  exit /b 1
)

echo.
echo OK — OTA1 written + selected. Power-cycle if UI looks old.
pause
