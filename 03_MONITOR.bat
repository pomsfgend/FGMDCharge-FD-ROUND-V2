@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
if not exist "%IDF_DIR%\export.bat" (
  echo ERROR: ESP-IDF 5.0.9 is not installed.
  echo Run BUILD_RELEASE.bat first.
  pause
  exit /b 1
)
call "%IDF_DIR%\export.bat"
set /p PORT=Enter cube COM port (example COM7):
if "%PORT%"=="" exit /b 1
idf.py -p "%PORT%" monitor
pause
