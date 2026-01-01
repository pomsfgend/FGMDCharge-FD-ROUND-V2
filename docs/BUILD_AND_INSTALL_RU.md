# Сборка и установка Release 2.8.7

## 1. Путь

Распакуйте архив в короткий путь без пробелов, скобок и кириллицы, например:

```text
C:\FGMDCharge_FD_ROUND_V2_RELEASE
```

## 2. Сборка

Запустите `BUILD_RELEASE.bat`.

Скрипт активирует ESP-IDF 5.0.9, восстанавливает совместимое Python-окружение,
удаляет старую конфигурацию и dependencies.lock, запускает host tests и static audit,
затем выполняет `idf.py set-target esp32c3`, `idf.py reconfigure`, `idf.py fullclean`
и `idf.py build`.

После успешной сборки:

```text
release\FGMDCharge_FD_ROUND_V2.bin
```

## 3. Первая загрузка

Используйте `FLASH_RELEASE_OTA1.bat`.

Он делает полный backup 4 MiB, проверяет app image, пишет только OTA1 (`0x1A0000`)
и только после отдельного подтверждения изменяет OTA selection.

## 4. Web

После запуска новой прошивки:

```text
SSID: FGMDCharge-V2
Password: FDcube2026
Address: http://192.168.4.1/
Key: main\config.h -> CUBE_WEB_KEY
```

## 5. Откат

`ROLLBACK_OTA0.bat` выбирает старый OTA0.
`RECOVERY_ORIGINAL.bat` полностью восстанавливает проверенный оригинальный dump.
