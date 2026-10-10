@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
call "%IDF_DIR%\export.bat"
if not defined CUBE_PORT set /p CUBE_PORT=Enter cube COM port (example COM14):
python tools\ota_slot.py --port "%CUBE_PORT%" ota1
echo Boot set to NEW firmware in ota_1. Reset the cube.
pause
