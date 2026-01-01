@echo off
setlocal EnableExtensions EnableDelayedExpansion
title FGMDCharge FD_ROUND V2 Build
cd /d "%~dp0"
set "PROJECT_DIR=%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"

echo.
echo ================================================================
echo  FGMDCharge / FD_ROUND V2  RELEASE 2.1.1
echo  ESP-IDF 5.0.9  /  ESP32-C3
echo ================================================================
echo.
echo Project folder:
echo   %PROJECT_DIR%
echo.

rem --- path safety (spaces only; avoid fragile regex) ---
set "PATH_CHECK=%PROJECT_DIR: =%"
if not "%PATH_CHECK%"=="%PROJECT_DIR%" (
  echo ERROR: Path contains SPACES.
  echo.
  echo Unpack to a short path WITHOUT spaces, for example:
  echo   C:\FGMDCharge_V2
  echo.
  echo Current path is not usable.
  goto FAIL
)

if not exist "%IDF_DIR%\export.bat" goto INSTALL_IDF
goto HAVE_IDF

:INSTALL_IDF
echo ESP-IDF 5.0.9 not found at:
echo   %IDF_DIR%
echo.
set "GIT_EXE="
where git >nul 2>&1
if not errorlevel 1 set "GIT_EXE=git"
if not defined GIT_EXE if exist "%ProgramFiles%\Git\cmd\git.exe" set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if not defined GIT_EXE if exist "%ProgramFiles(x86)%\Git\cmd\git.exe" set "GIT_EXE=%ProgramFiles(x86)%\Git\cmd\git.exe"

if not defined GIT_EXE (
  echo ERROR: Git is required to install ESP-IDF.
  echo Install Git for Windows from https://git-scm.com/download/win
  echo then run this script again.
  goto FAIL
)

if not exist "%USERPROFILE%\esp" mkdir "%USERPROFILE%\esp"
if exist "%IDF_DIR%" if not exist "%IDF_DIR%\export.bat" (
  echo Removing incomplete ESP-IDF folder...
  rmdir /s /q "%IDF_DIR%" 2>nul
)

if not exist "%IDF_DIR%" (
  echo Cloning ESP-IDF v5.0.9 (this may take several minutes)...
  "%GIT_EXE%" clone --recursive --depth 1 --branch v5.0.9 https://github.com/espressif/esp-idf.git "%IDF_DIR%"
  if errorlevel 1 goto FAIL
)

echo Installing ESP-IDF tools for esp32c3...
call "%IDF_DIR%\install.bat" esp32c3
if errorlevel 1 goto FAIL

:HAVE_IDF
echo Activating ESP-IDF environment...
call "%IDF_DIR%\export.bat"
if errorlevel 1 (
  echo ERROR: export.bat failed.
  goto FAIL
)

cd /d "%PROJECT_DIR%"
if errorlevel 1 goto FAIL

echo.
echo Cleaning previous build state...
if exist sdkconfig del /q sdkconfig 2>nul
if exist sdkconfig.old del /q sdkconfig.old 2>nul
if exist dependencies.lock del /q dependencies.lock 2>nul
if exist build rmdir /s /q build 2>nul
if not exist release mkdir release
if exist release\FGMDCharge_FD_ROUND_V2.bin del /q release\FGMDCharge_FD_ROUND_V2.bin 2>nul
if exist release\bootloader.bin del /q release\bootloader.bin 2>nul
if exist release\partition-table.bin del /q release\partition-table.bin 2>nul

echo Checking Python / tools...
where python >nul 2>&1
if errorlevel 1 (
  echo ERROR: python not found in PATH after IDF export.
  goto FAIL
)

python -m pip install --disable-pip-version-check -r requirements-tools.txt
if errorlevel 1 goto FAIL

python -m esptool version
if errorlevel 1 goto FAIL

echo Generating web assets...
python tools\embed_web_assets.py
if errorlevel 1 goto FAIL

echo Running host tests...
python host_tests\test_core.py
if errorlevel 1 goto FAIL

echo Running static audit...
python tools\static_audit.py
if errorlevel 1 goto FAIL

python tools\verify_release.py
if errorlevel 1 goto FAIL

echo.
echo Configuring target esp32c3...
idf.py set-target esp32c3
if errorlevel 1 goto FAIL

python tools\force_sdkconfig.py
if errorlevel 1 goto FAIL

idf.py reconfigure
if errorlevel 1 goto FAIL

echo Full clean...
idf.py fullclean
if errorlevel 1 goto FAIL

echo.
echo Forcing sdkconfig policy before build...
python tools\force_sdkconfig.py
if errorlevel 1 goto FAIL

echo Building (this takes a few minutes)...
idf.py build
if errorlevel 1 goto FAIL

if not exist build\FD_ROUND_V2.bin (
  echo ERROR: build\FD_ROUND_V2.bin was not created.
  goto FAIL
)

echo Copying release artifacts...
copy /y build\FD_ROUND_V2.bin release\FGMDCharge_FD_ROUND_V2.bin >nul
if exist build\bootloader\bootloader.bin copy /y build\bootloader\bootloader.bin release\bootloader.bin >nul
if exist build\partition_table\partition-table.bin copy /y build\partition_table\partition-table.bin release\partition-table.bin >nul
copy /y partitions.csv release\partitions.csv >nul

python tools\check_app_image.py release\FGMDCharge_FD_ROUND_V2.bin
if errorlevel 1 goto FAIL

python tools\verify_release.py
if errorlevel 1 goto FAIL

python tools\static_audit.py
if errorlevel 1 goto FAIL

for %%F in (release\FGMDCharge_FD_ROUND_V2.bin) do set APP_SIZE=%%~zF
set "APP_SHA="
for /f "skip=1 tokens=*" %%H in ('certutil -hashfile release\FGMDCharge_FD_ROUND_V2.bin SHA256 ^| findstr /r /v "CertUtil"') do (
  if not defined APP_SHA set "APP_SHA=%%H"
)

(
  echo FGMDCharge / FD_ROUND V2
  echo Release: 2.1.1
  echo Target: ESP32-C3
  echo ESP-IDF: 5.0.9
  echo Application: release\FGMDCharge_FD_ROUND_V2.bin
  echo Size: !APP_SIZE! bytes
  echo SHA256: !APP_SHA!
  echo Built by BUILD_RELEASE.bat
) > release\BUILD_INFO.txt

python tools\write_release_manifest.py
if errorlevel 1 goto FAIL

echo.
echo ================================================================
echo  BUILD OK
echo  Firmware: %PROJECT_DIR%release\FGMDCharge_FD_ROUND_V2.bin
echo  Size:     !APP_SIZE! bytes
echo ================================================================
echo  Next step: run FLASH_RELEASE_OTA1.bat
echo ================================================================
echo.
pause
exit /b 0

:FAIL
echo.
echo ================================================================
echo  BUILD FAILED
echo  Do NOT flash anything from this run.
echo ================================================================
echo.
pause
exit /b 1
