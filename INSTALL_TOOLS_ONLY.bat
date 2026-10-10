@echo off
setlocal
cd /d "%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
if not exist "%IDF_DIR%\install.bat" (echo ERROR: ESP-IDF 5.0.9 is not installed. Run BUILD_RELEASE.bat first.&pause&exit /b 1)
echo Repairing ESP-IDF 5.0.9 Python environment...
call "%IDF_DIR%\install.bat" esp32c3
if errorlevel 1 (echo ESP-IDF tool installation/repair failed.&pause&exit /b 1)
call "%IDF_DIR%\export.bat"
if errorlevel 1 (echo ERROR: Could not activate ESP-IDF environment.&pause&exit /b 1)
python -m pip install --disable-pip-version-check -r requirements-tools.txt
if errorlevel 1 (echo Compatible Python package installation failed.&pause&exit /b 1)
python -m esptool version
if errorlevel 1 (echo esptool check failed.&pause&exit /b 1)
echo.
echo ESP-IDF 5.0.9 environment repaired.
pause
