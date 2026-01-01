@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
if not exist "%IDF_DIR%\export.bat" (echo ERROR: ESP-IDF 5.0.9 is not installed. Run BUILD_RELEASE.bat first.&pause&exit /b 1)
call "%IDF_DIR%\export.bat"
if errorlevel 1 (echo ERROR: Could not activate ESP-IDF environment.&pause&exit /b 1)
if not exist original\dump1.bin (
  echo ERROR: original\dump1.bin is missing.
  pause
  exit /b 1
)
where python >nul 2>&1 || (echo Python not found. Run BUILD_RELEASE.bat first.&pause&exit /b 1)
python -m esptool version >nul 2>&1 || (echo esptool not found. Run BUILD_RELEASE.bat first.&pause&exit /b 1)
set /p PORT=Enter cube COM port (example COM7):
if "%PORT%"=="" exit /b 1
certutil -hashfile original\dump1.bin SHA256
choice /C YN /N /M "DANGER: overwrite entire 4 MiB with ORIGINAL dump1.bin? [Y/N] "
if errorlevel 2 exit /b 0
python tools\verify_original_dump.py
if errorlevel 1 (echo Refusing restore: original dump verification failed.&pause&exit /b 1)
python -m esptool --chip esp32c3 --port "%PORT%" write-flash 0x0 original\dump1.bin
if errorlevel 1 (echo RECOVERY FAILED.&pause&exit /b 1)
echo ORIGINAL FLASH RESTORED.
pause
