@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
if not exist "%IDF_DIR%\export.bat" (
  echo ERROR: ESP-IDF 5.0.9 not found. Run BUILD_RELEASE.bat once first.
  pause
  exit /b 1
)
call "%IDF_DIR%\export.bat"
if errorlevel 1 (
  echo ERROR: Could not activate ESP-IDF.
  pause
  exit /b 1
)
set "APP=%~dp0original\ota0_from_dump1.bin"
if not exist "%APP%" (
  echo ERROR: missing %APP%
  echo Copy ota0_from_dump1.bin into original\
  pause
  exit /b 1
)
if not defined CUBE_PORT set /p CUBE_PORT=Enter cube COM port (example COM14):
if "%CUBE_PORT%"=="" (
  echo ERROR: COM port is empty.
  pause
  exit /b 1
)
echo Writing original app to ota_0 at 0x10000 only. ota_1 is not touched.
python -m esptool --chip esp32c3 -p %CUBE_PORT% -b 460800 --before default_reset --after hard_reset write_flash 0x10000 "%APP%"
if errorlevel 1 (
  echo FLASH FAILED
  pause
  exit /b 1
)
echo Done. ota_1 was not overwritten.
echo Three quick resets toggle ota_0 / ota_1.
pause
