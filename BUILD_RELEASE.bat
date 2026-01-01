@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
set "PROJECT_DIR=%~dp0"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
set "GIT_EXE="

echo %PROJECT_DIR% | findstr /R "[^ -~]" >nul
if not errorlevel 1 (
  echo ERROR: project path contains non-ASCII characters (Cyrillic etc).
  echo Move this folder to a pure ASCII path, e.g. C:\FGMDCharge_V2
  pause
  exit /b 1
)
if not "%PROJECT_DIR: =%"=="%PROJECT_DIR%" (
  echo ERROR: project path contains spaces.
  echo Move this folder to something like C:\FGMDCharge_V2
  pause
  exit /b 1
)

if not exist "%IDF_DIR%\export.bat" goto INSTALL_IDF
goto HAVE_IDF

:INSTALL_IDF
where git >nul 2>&1
if not errorlevel 1 set "GIT_EXE=git"
if exist "%ProgramFiles%\Git\cmd\git.exe" if not defined GIT_EXE set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if defined GIT_EXE goto CLONE_IDF

where winget >nul 2>&1
if errorlevel 1 (
  echo ERROR: Git is required for the first installation of ESP-IDF 5.0.9.
  echo Install Git for Windows, then run BUILD_RELEASE.bat again.
  pause
  exit /b 1
)

echo Installing Git for Windows via winget...
winget install --id Git.Git -e --source winget --accept-package-agreements --accept-source-agreements
if exist "%ProgramFiles%\Git\cmd\git.exe" set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
if not defined GIT_EXE (
  echo ERROR: Git was installed but git.exe was not found.
  echo Close this window and run BUILD_RELEASE.bat again.
  pause
  exit /b 1
)

:CLONE_IDF
if not exist "%USERPROFILE%\esp" mkdir "%USERPROFILE%\esp"
if exist "%IDF_DIR%" if not exist "%IDF_DIR%\export.bat" (
  echo Removing incomplete ESP-IDF directory...
  rmdir /s /q "%IDF_DIR%"
  if errorlevel 1 goto FAIL
)
if not exist "%IDF_DIR%" (
  echo Downloading ESP-IDF v5.0.9...
  "%GIT_EXE%" clone --recursive --depth 1 --branch v5.0.9 https://github.com/espressif/esp-idf.git "%IDF_DIR%"
  if errorlevel 1 goto FAIL
)

echo Installing ESP-IDF 5.0.9 tools for ESP32-C3...
call "%IDF_DIR%\install.bat" esp32c3
if errorlevel 1 goto FAIL

:HAVE_IDF
call "%IDF_DIR%\install.bat" esp32c3
if errorlevel 1 goto FAIL
call "%IDF_DIR%\export.bat"
if errorlevel 1 goto FAIL

cd /d "%PROJECT_DIR%"
echo.
echo ================================================================
echo FGMDCharge / FD_ROUND V2 RELEASE 2.0.5
echo Deterministic clean configure + build
echo ESP-IDF 5.0.9 / ESP32-C3
echo ================================================================
echo.

rem Force the checked-in defaults to take effect instead of inheriting an old sdkconfig.
if exist sdkconfig del /q sdkconfig
if exist sdkconfig.old del /q sdkconfig.old
if exist dependencies.lock del /q dependencies.lock
if exist build rmdir /s /q build
if not exist release mkdir release
if exist release\FGMDCharge_FD_ROUND_V2.bin del /q release\FGMDCharge_FD_ROUND_V2.bin
if exist release\bootloader.bin del /q release\bootloader.bin
if exist release\partition-table.bin del /q release\partition-table.bin

where python >nul 2>&1
if errorlevel 1 goto FAIL
python -m pip install --disable-pip-version-check -r requirements-tools.txt
if errorlevel 1 goto FAIL
python -m esptool version
if errorlevel 1 goto FAIL
python tools\embed_web_assets.py
if errorlevel 1 goto FAIL

python host_tests\test_core.py
if errorlevel 1 goto FAIL
python tools\static_audit.py
if errorlevel 1 goto FAIL
python tools\verify_release.py
if errorlevel 1 goto FAIL

idf.py set-target esp32c3
if errorlevel 1 goto FAIL
python tools\force_sdkconfig.py
if errorlevel 1 goto FAIL
idf.py reconfigure
if errorlevel 1 goto FAIL
idf.py fullclean
if errorlevel 1 goto FAIL
idf.py build
if errorlevel 1 goto FAIL

if not exist build\FD_ROUND_V2.bin (
  echo ERROR: build finished but build\FD_ROUND_V2.bin is missing.
  goto FAIL
)

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
for /f "skip=1 tokens=*" %%H in ('certutil -hashfile release\FGMDCharge_FD_ROUND_V2.bin SHA256 ^| findstr /r /v "CertUtil"') do if not defined APP_SHA set "APP_SHA=%%H"
(
  echo FGMDCharge / FD_ROUND V2
  echo Release: 2.0.5
  echo Target: ESP32-C3
  echo ESP-IDF: 5.0.9
  echo Application: release\FGMDCharge_FD_ROUND_V2.bin
  echo Size: !APP_SIZE! bytes
  echo SHA256: !APP_SHA!
  echo Built by BUILD_RELEASE.bat after clean sdkconfig/dependency state.
)>release\BUILD_INFO.txt

python tools\write_release_manifest.py
if errorlevel 1 goto FAIL

echo.
echo ================================================================
echo BUILD OK — FGMDCharge / FD_ROUND V2 RELEASE 2.0.5
echo Firmware: %PROJECT_DIR%release\FGMDCharge_FD_ROUND_V2.bin
echo Size:     !APP_SIZE! bytes
echo ================================================================
echo Next: FLASH_RELEASE_OTA1.bat
echo.
pause
exit /b 0

:FAIL
echo.
echo BUILD FAILED. Do not flash a partial result.
echo See the error output above.
pause
exit /b 1
