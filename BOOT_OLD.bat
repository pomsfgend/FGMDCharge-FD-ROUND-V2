@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
call "%IDF_DIR%\export.bat"
if not defined CUBE_PORT set /p CUBE_PORT=Enter cube COM port (example COM14):
python tools\ota_slot.py --port "%CUBE_PORT%" ota0
echo Boot set to ORIGINAL firmware in ota_0. Reset the cube.
pause
