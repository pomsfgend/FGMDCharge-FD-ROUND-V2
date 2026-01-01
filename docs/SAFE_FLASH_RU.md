# Безопасная первая загрузка Release 2.7.6

Канонический сценарий: `FLASH_RELEASE_OTA1.bat`.

Перед записью сохраняется весь 4 MiB flash. Новый app image проходит локальную проверку,
после чего записывается только в OTA1 по `0x1A0000`. Bootloader, partition table, NVS и OTA0
не перезаписываются.

Не используйте `idf.py flash` для первого теста этого релиза.

Откат: `ROLLBACK_OTA0.bat`.
Полное восстановление: `RECOVERY_ORIGINAL.bat`.
