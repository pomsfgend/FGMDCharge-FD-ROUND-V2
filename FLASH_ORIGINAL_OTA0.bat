@echo off
set PORT=COM14
echo Writing original dump1 app to ota_0 only. ota_1 is not touched.
python -m esptool --chip esp32c3 -p %PORT% -b 460800 write_flash 0x10000 original\ota0_from_dump1.bin
echo Done. Three quick resets toggle ota_0 / ota_1.
