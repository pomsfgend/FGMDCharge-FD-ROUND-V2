@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
if not exist "%IDF_DIR%\export.bat" (echo ERROR: IDF missing&pause&exit /b 1)
call "%IDF_DIR%\export.bat"
if errorlevel 1 (pause&exit /b 1)

set "APP=%~dp0release\FGMDCharge_FD_ROUND_V2.bin"
if not exist "%APP%" (
  echo ERROR: run QUICK_BUILD.bat or BUILD_RELEASE.bat first
  pause
  exit /b 1
)

set /p PORT=Enter COM port (e.g. COM14): 
if "%PORT%"=="" (echo empty port&pause&exit /b 1)

echo.
echo Fast OTA1 flash only (no 4MB backup) at 921600 baud
python -m esptool --chip esp32c3 --port "%PORT%" -b 921600 write_flash 0x1A0000 "%APP%"
if errorlevel 1 (echo FLASH FAILED&pause&exit /b 1)

python tools\ota_slot.py --port "%PORT%" ota1
if errorlevel 1 (echo OTA slot select failed&pause&exit /b 1)

echo.
echo OK — OTA1 written and selected. Reset the cube.
pause
