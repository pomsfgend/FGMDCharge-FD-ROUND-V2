# Zip catalog

Still not the whole cube folder. Exact duplicate uploads are not stored twice.
When two files share a name but differ, the second is `*-alt.zip`.
Dumps that match `original/` are not copied again.
`archive/reference/` is upstream J1772 [dzurikmiroslav/esp32-evse](https://github.com/dzurikmiroslav/esp32-evse), GPL-3, not this cube.
Trust the config.h column when it disagrees with the file name.

| File | Bytes | config.h | Top folder |
|---|---:|---|---|
| `CUBIK_DISPLAY_LCD_RE_FROM_DUMP.zip` | 8966 | — | `lcd_re` |
| `CUBIK_FIRMWARE_RE_FINAL_ANALYTICS.zip` | 1424355 | — | `README.md` |
| `FGMDCharge_ALL_NOTES_AND_2.7.4.zip` | 4767762 | — | `FGMDCharge_FD_ROUND_V2_2.7.4.zip` |
| `FGMDCharge_FD_ROUND_V2_2.1.0.zip` | 4092700 | 2.1.1 | `FGMDCharge_FD_ROUND_V2_2.1.1` |
| `FGMDCharge_FD_ROUND_V2_2.1.1.zip` | 4092942 | 2.1.2 | `FGMDCharge_FD_ROUND_V2_2.1.2` |
| `FGMDCharge_FD_ROUND_V2_2.1.2.zip` | 4092942 | 2.1.2 | `FGMDCharge_FD_ROUND_V2_2.1.2` |
| `FGMDCharge_FD_ROUND_V2_2.1.3.zip` | 4093831 | 2.1.3 | `FGMDCharge_FD_ROUND_V2_2.1.3` |
| `FGMDCharge_FD_ROUND_V2_2.1.4.zip` | 4093772 | 2.1.4 | `FGMDCharge_FD_ROUND_V2_2.1.4` |
| `FGMDCharge_FD_ROUND_V2_2.3.0.zip` | 4077511 | 2.3.0 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.3.1.zip` | 4079505 | 2.3.1 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.3.2.zip` | 4079987 | 2.3.2 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.3.3.zip` | 4080330 | 2.3.3 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.3.4.zip` | 4081351 | 2.3.4 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.3.5.zip` | 4082080 | 2.3.5 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.3.6.zip` | 4082211 | 2.3.6 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.3.7.zip` | 4082307 | 2.3.7 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.3.8.zip` | 4082601 | 2.3.8 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.3.9.zip` | 4082765 | 2.3.9 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.4.0.zip` | 4083072 | 2.4.0 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.4.1.zip` | 4080579 | 2.4.1 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.4.2.zip` | 4081298 | 2.4.2 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.4.3.zip` | 4081549 | 2.4.3 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.4.4.zip` | 4081531 | 2.4.4 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.4.5.zip` | 4081540 | 2.4.5 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.4.6.zip` | 4081650 | 2.4.6 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.4.7.zip` | 4081657 | 2.4.7 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.4.8.zip` | 4081887 | 2.4.8 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.4.9.zip` | 4755430 | 2.4.9 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.0.zip` | 4755426 | 2.5.0 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.1.zip` | 4757960 | 2.5.1 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.2.zip` | 4757025 | 2.5.2 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.3 (1).zip` | 4758606 | 2.5.3 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.3 (2).zip` | 4758619 | 2.5.3 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.3-alt.zip` | 4757676 | 2.5.3 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.3.zip` | 4758576 | 2.5.3 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.4.zip` | 4758812 | 2.5.4 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.5-alt.zip` | 4759263 | 2.5.5 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.5.zip` | 4759257 | 2.5.5 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.6.zip` | 4760225 | 2.5.6 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.7.zip` | 4760744 | 2.5.7 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.8.zip` | 4774930 | 2.5.8 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.5.9.zip` | 4775621 | 2.5.9 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.6.0.zip` | 4776194 | 2.6.0 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.7.0.zip` | 4778061 | 2.7.0 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.7.1.zip` | 4778157 | 2.7.1 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.7.2.zip` | 4778368 | 2.7.2 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.7.3.zip` | 4778453 | 2.7.3 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.7.4.zip` | 4778558 | 2.7.4 | `FGMDCharge_V2` |
| `FGMDCharge_FD_ROUND_V2_2.7.5.zip` | 4707832 | 2.7.5 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.7.6 (1).zip` | 4707990 | 2.7.6 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.7.6 (2).zip` | 4707991 | 2.7.6 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.7.6 (3).zip` | 4707997 | 2.7.6 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.7.6-alt.zip` | 4707982 | 2.7.6 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.7.6.zip` | 4707997 | 2.7.6 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.7.7-alt.zip` | 4707872 | 2.7.7 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.7.7.zip` | 4707888 | 2.7.7 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.7.8.zip` | 4707963 | 2.7.8 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.7.9-alt.zip` | 4707971 | 2.7.9 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.7.9.zip` | 4707976 | 2.7.9 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.0.zip` | 4708040 | 2.8.0 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.1.zip` | 4708251 | 2.8.1 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.10.zip` | 5461227 | 2.8.10 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.11.zip` | 5469971 | 2.8.11 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.12.zip` | 5478988 | 2.8.12 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.13 (1).zip` | 5480443 | 2.8.13 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.13.zip` | 5480336 | 2.8.13 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.14.zip` | 5493584 | 2.8.14 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.15.zip` | 5498765 | 2.8.15 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.15_release.zip` | 6239971 | 2.8.15 | `.gitignore` |
| `FGMDCharge_FD_ROUND_V2_2.8.2.zip` | 4708279 | 2.8.2 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.3.zip` | 4708238 | 2.8.3 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.4.zip` | 4708210 | 2.8.4 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.5.zip` | 4708206 | 2.8.5 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.6.zip` | 4708327 | 2.8.6 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.7.zip` | 4710548 | 2.8.7 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.8.zip` | 4711900 | 2.8.8 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_2.8.9.zip` | 5459927 | 2.8.9 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_FIXED.zip` | 4711552 | 2.7.4 | `final` |
| `FGMDCharge_FD_ROUND_V2_FIXED_2.0.4.zip` | 4068473 | 2.0.4 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_FIXED_2.0.5.zip` | 4084235 | 2.0.5 | `03_MONITOR.bat` |
| `FGMDCharge_FD_ROUND_V2_RECONSTRUCTED_2.0.6_FINAL.zip` | 4079830 | 2.0.6 | `FGMDCharge_FD_ROUND_V2_RECONSTRUCTED_2.0.6` |
| `FGMDCharge_FD_ROUND_V2grok.zip` | 4707268 | 2.7.4 | `03_MONITOR.bat` |
| `FGMDCharge_FULL_2.7.4_AND_SUMMARYgrok.zip` | 4712334 | 2.7.4 | `FGMDCharge_V2` |
| `cubik_firmware_reverse_engineering.zip` | 2094723 | — | `docs` |
| `cubik_firmware_reverse_engineering_step17.zip` | 2959029 | — | `cubik_firmware_reverse_engineering_step9` |
| `cubik_firmware_reverse_engineering_step18.zip` | 2965379 | — | `cubik_firmware_reverse_engineering_step9` |
| `cubik_firmware_reverse_engineering_step19.zip` | 2972553 | — | `cubik_firmware_reverse_engineering_step9` |
| `cubik_firmware_reverse_engineering_step20.zip` | 2979497 | — | `cubik_firmware_reverse_engineering_step9` |
| `cubik_firmware_reverse_engineering_step21.zip` | 2987874 | — | `cubik_firmware_reverse_engineering_step9` |
