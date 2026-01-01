@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ================================================================
echo  QUICK BUILD (no fullclean) — FD_ROUND V2
echo ================================================================
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
if not exist "%IDF_DIR%\export.bat" (
  echo ERROR: ESP-IDF 5.0.9 missing
  pause
  exit /b 1
)
call "%IDF_DIR%\export.bat"
if errorlevel 1 (echo ERROR: export failed&pause&exit /b 1)

python tools\embed_web_assets.py
if errorlevel 1 (echo embed failed&pause&exit /b 1)

if exist sdkconfig (
  python tools\force_sdkconfig.py
)

idf.py build
if errorlevel 1 (
  echo BUILD FAILED
  pause
  exit /b 1
)

if not exist release mkdir release
copy /y build\FD_ROUND_V2.bin release\FGMDCharge_FD_ROUND_V2.bin >nul
echo.
echo QUICK BUILD OK: release\FGMDCharge_FD_ROUND_V2.bin
echo Flash with FLASH_RELEASE_OTA1.bat  (or FLASH_QUICK.bat)
pause
