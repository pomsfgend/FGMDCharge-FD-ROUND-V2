@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
if not exist "%IDF_DIR%\install.bat" (
  echo ERROR: ESP-IDF 5.0.9 not found at %IDF_DIR%
  echo Run BUILD_RELEASE.bat first so ESP-IDF can be installed.
  pause
  exit /b 1
)
echo.
echo ================================================================
echo Repair ESP-IDF 5.0.9 Python environment
echo This restores the versions required by IDF 5.0.x.
echo ================================================================
echo.
call "%IDF_DIR%\install.bat" esp32c3
if errorlevel 1 goto FAIL
call "%IDF_DIR%\export.bat"
if errorlevel 1 goto FAIL
python -m pip install --disable-pip-version-check -r requirements-tools.txt
if errorlevel 1 goto FAIL
python -m esptool version
if errorlevel 1 goto FAIL
echo.
echo REPAIR OK.
echo Expected esptool: 4.7.0
echo Expected cryptography: 36.0.2
echo Now run BUILD_RELEASE.bat
pause
exit /b 0
:FAIL
echo.
echo REPAIR FAILED. Read the error above.
pause
exit /b 1
