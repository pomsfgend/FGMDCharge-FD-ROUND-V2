FGMDCharge / FD_ROUND V2 — Release 2.0.6 firmware artifacts

After a successful BUILD_RELEASE.bat:
  FGMDCharge_FD_ROUND_V2.bin — application image for OTA slot
  bootloader.bin              — diagnostic copy of the newly built bootloader
  partition-table.bin         — diagnostic copy of the partition table
  partitions.csv              — matching source partition definition
  BUILD_INFO.txt              — build size/hash information

Do not manually flash these files as a combined whole-chip image during the first test.
Use FLASH_RELEASE_OTA1.bat so the original OTA0 is preserved.
