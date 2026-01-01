@echo off
setlocal
call "%IDF_PATH%\export.bat" >nul 2>&1
if "%IDF_PATH%"=="" (
  call C:\esp\esp\esp-idf-5.0.9\export.bat
)
set /p PORT=COM port (e.g. COM14): 
echo Erasing NVS (0x9000) and phy_init (0xF000) on %PORT% ...
python -m esptool --chip esp32c3 -p %PORT% -b 460800 erase_region 0x9000 0x5000
python -m esptool --chip esp32c3 -p %PORT% -b 460800 erase_region 0xF000 0x1000
echo Done. Power-cycle the cube, then flash app if needed.
pause
