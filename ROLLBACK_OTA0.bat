@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
if not exist "%IDF_DIR%\export.bat" (echo ERROR: ESP-IDF 5.0.9 is not installed. Run BUILD_RELEASE.bat first.&pause&exit /b 1)
call "%IDF_DIR%\export.bat"
if errorlevel 1 (echo ERROR: Could not activate ESP-IDF environment.&pause&exit /b 1)
where python >nul 2>&1 || (echo Python not found.&pause&exit /b 1)
python -m esptool version >nul 2>&1 || (echo esptool not found. Run BUILD_RELEASE.bat first.&pause&exit /b 1)
set /p PORT=Enter cube COM port (example COM7):
if "%PORT%"=="" exit /b 1
python tools\ota_slot.py --port "%PORT%" ota0
if errorlevel 1 (echo ROLLBACK FAILED.&pause&exit /b 1)
echo OTA0 selected. Reset the cube.
pause
