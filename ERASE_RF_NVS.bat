@echo off
REM Erase NVS + phy_init only (keeps OTA apps). Fixes dead SoftAP after many flashes.
set /p PORT=COM port (example COM14): 
call C:\esp\esp\esp-idf-5.0.9\export.bat
python -m esptool --chip esp32c3 -p %PORT% erase_region 0x9000 0x5000
echo Done. Power-cycle the cube, then FLASH_RELEASE_OTA1.bat if needed.
pause
