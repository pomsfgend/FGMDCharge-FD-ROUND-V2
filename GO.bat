@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "PORT=COM14"
set "IDF_DIR=%USERPROFILE%\esp\esp-idf-5.0.9"
echo.
echo  FGMDCharge / FD_ROUND V2  RELEASE 2.8.13
echo  1. Build
echo  2. One flash: bootloader + partition + original ota_0 + our ota_1 + factory backup + boot slot
echo  3. Monitor. Then YOU power the cube.
echo  Port %PORT%  baud 921600. No reset between steps.
echo.
call "%IDF_DIR%\export.bat"
if errorlevel 1 goto fail
if exist build rmdir /s /q build
if exist build (
  echo Close the serial monitor, delete C:\FGMDCharge_V2\build, run GO.bat again.
  goto fail
)
echo.| call BUILD_RELEASE.bat
if not exist "build\bootloader\bootloader.bin" goto fail
if not exist "release\FGMDCharge_FD_ROUND_V2.bin" goto fail
if not exist "original\ota0_from_dump1.bin" goto fail
if not exist "original\factory_lzss.bin" goto fail
python -c "import struct,zlib,pathlib; seq=2; e=bytearray(b'\xFF'*32); struct.pack_into('<I',e,0,seq); struct.pack_into('<I',e,28,zlib.crc32(struct.pack('<I',seq),0xFFFFFFFF)&0xFFFFFFFF); b=bytearray(b'\xFF'*8192); b[0:32]=e; b[32:64]=e; pathlib.Path('build/otadata_ota1.bin').write_bytes(b)"
python -m esptool --chip esp32c3 -p %PORT% -b 921600 --before default_reset --after no_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0xD000 build\otadata_ota1.bin 0x10000 original\ota0_from_dump1.bin 0x1A0000 release\FGMDCharge_FD_ROUND_V2.bin 0x330000 original\factory_lzss.bin
if errorlevel 1 goto fail
echo.
echo  FLASH DONE. Monitor is opening. Then YOU power the cube.
echo  Power-on opens the menu: НОВАЯ or СТАРАЯ.
echo  СТАРАЯ stays in the original firmware until you cut power.
echo  Next power-on opens the menu again.
echo  Wi-Fi: Cube  password 12345678  channel 6  http://192.168.4.1/
echo  OTA from the phone: release\FGMDCharge_FD_ROUND_V2.bin  (not the zip).
echo  Do not cut power until the cube reboots by itself.
echo  Screen password 1234 is remembered until power-off.
echo  If the phone says wrong password, forget Cube and join again.
echo.
idf.py -p %PORT% -b 115200 monitor
exit /b 0
:fail
echo FAILED. Copy this whole window to the chat.
pause
exit /b 1
