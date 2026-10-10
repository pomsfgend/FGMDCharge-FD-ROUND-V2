@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
call "%IDF_DIR%\export.bat"
set "BIN=%~dp0build\bootloader\bootloader.bin"
if not exist "%BIN%" (
  echo ERROR: build\bootloader\bootloader.bin missing. Run BUILD_RELEASE.bat first.
  pause
  exit /b 1
)
if not defined CUBE_PORT set /p CUBE_PORT=Enter cube COM port (example COM14):
echo Writing bootloader only at 0x0. App slots are not touched.
python -m esptool --chip esp32c3 -p %CUBE_PORT% -b 460800 --before default_reset --after hard_reset write_flash 0x0 "%BIN%"
pause
