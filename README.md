# FGMDCharge / FD_ROUND V2

Public firmware for the round ESP32-C3 cube that answers a DC charging station on CAN as the vehicle BMS.

Release **2.8.17**. Journal of every earlier build: [CHANGELOG.md](CHANGELOG.md). Branch rules: [docs/BRANCHES.md](docs/BRANCHES.md). This repository's firmware, build scripts and documentation were written by **Grok (xAI)**. The round board and the factory image were already on the device. Grok did not design that PCB and did not write the factory firmware.

[Русский](#русский) is below.

## Who made the hardware

The manufacturer is **not known**.

The factory image was read off the cube. Readable strings in it say:

| Where | Text |
|---|---|
| ESP-IDF project name | `FD_ROUND` |
| On-screen title | `EVCHARGER` |
| BLE name in the image | `FGMDCharge` |
| Chip | ESP32-C3 |
| Screen log in the factory image | ILI9341 (the image logs that name) |

No company name, copyright line or website is in that image. Treat `original/` as a dump of whatever shipped in this cube, not as a work of a named vendor.

## Factory image

`original/` is published so the project can be repeated and so **СТАРАЯ** can still be flashed. It is **not** under the MIT license. See [LICENSE](LICENSE).

| File | What it is |
|---|---|
| `dump1.bin` | 4 MB read of the chip |
| `bootloader_0x1000_0x8000.bin` | factory bootloader |
| `partition_table_0x8000_0x9000.bin` | factory partition table |
| `nvs_0x9000_0xD000.bin` | factory NVS (may hold that device's old settings) |
| `otadata_0xD000_0xF000.bin` | factory OTA boot selection |
| `phy_init_0xF000_0x10000.bin` | PHY init data |
| `ota0_partition_0x190000.bin` | factory app slot, full partition |
| `ota0_from_dump1.bin` | factory app image cut from the dump |
| `ota0_irom.bin`, `ota0_tail.bin` | slices of that app |
| `ota1_partition_0x190000.bin`, `ota1_irom.bin` | second slot as shipped |
| `factory_lzss.bin` | compressed copy used by the boot menu |

OTA of `release/FGMDCharge_FD_ROUND_V2.bin` does not need these files. `GO.bat` does, because it writes the factory image back into `ota_0`.

## What this firmware does

It does not start a charge by itself as a charger. It answers the station. CAN transmit is on. Protocol **auto** follows the first frame:

| Station sends | Protocol |
|---|---|
| Version frame, addresses 0x5F / 0x25 | GB/T 27930-2023 class B (ChaoJi) |
| CHM | GB/T 27930-2015 and 2023 class A |
| CRM, and neither of the above | GB/T 27930-2011 |

Charge current is encoded negative, as GB/T requires. A CCS pause does not end the session. CST, CEM and CSD do.

Wi-Fi **Cube**, password `12345678`, site `http://192.168.4.1/`. The screen code checked in firmware is `1234`. It is not printed on the display.

Prebuilt app image (1 143 440 bytes, SHA256 `7a6ca69a24e2f7c1fb3f312d40313c5862d78a20a244591a26b8cd08b8d263ac`): [`release/FGMDCharge_FD_ROUND_V2.bin`](https://github.com/pomsfgend/FGMDCharge-FD-ROUND-V2/raw/main/release/FGMDCharge_FD_ROUND_V2.bin).

## Parts to build the same thing

Logic is 3.3 V. Do not power a relay coil from a GPIO pin. Use the transistor or driver already on the board, or add one.

| Part | Used here | Notes |
|---|---|---|
| Module | ESP32-C3, 4 MB flash | This tree is built for this chip. ESP-IDF **5.0.9** only (`idf: >=5.0.9,<5.1.0`). |
| Display | SPI ST7789, 240×280 | This firmware drives ST7789 (invert on, BGR, gap Y = 20). The factory image's log text says ILI9341. Match the panel you actually have. |
| Touch | CST816S, I2C address `0x15` | Chip ID seen on the cube: `0xB6`. |
| CAN | 3.3 V CAN transceiver to CANH/CANL | Part number is not in this repo. 250 kbit/s. |
| Relays | two outputs | GPIO high = on. GPIO6 prepare, GPIO7 load. |
| Backlight | LED PWM | GPIO2, 5 kHz, default 70%. |
| USB | external USB-UART on UART0 | Not native USB. See the warning below. |
| Software | ESP-IDF 5.0.9, LVGL **8.3.11**, Python 3.11 on Windows as used to build | LVGL is fetched by the IDF component manager. `components/esp_lcd_ili9341` is vendored. |

### Do not short RX and TX

On this ESP32-C3 the console is UART0: **TX = GPIO21**, **RX = GPIO20**. Those pins go to the USB-UART chip. Shorting RX to TX can destroy the ESP or the USB-UART. GPIO18 and GPIO19 are the display, not USB.

## Pins (ESP32-C3, this cube)

All numbers are GPIO numbers in `main/config.h`.

| Function | GPIO | Direction |
|---|---|---|
| LCD MOSI | 5 | out |
| LCD SCLK | 19 | out |
| LCD CS | 4 | out |
| LCD DC | 18 | out |
| LCD backlight PWM | 2 | out |
| LCD reset | not wired (`-1`) | |
| LCD MISO | not wired | |
| Touch SDA | 1 | I2C |
| Touch SCL | 8 | I2C |
| CAN TX | 10 | to transceiver TXD |
| CAN RX | 9 | from transceiver RXD |
| Relay A | 6 | out, prepare |
| Relay B | 7 | out, load |
| UART0 TX | 21 | console, IDF default |
| UART0 RX | 20 | console, IDF default |

SPI clock 40 MHz. CAN 250 kbit/s, extended frames, accept-all filter.

### Flash map (4 MB)

| Name | Offset | Size |
|---|---|---|
| bootloader | `0x1000` | |
| partition table | `0x8000` | |
| nvs | `0x9000` | 16 KB |
| otadata | `0xD000` | 8 KB |
| phy_init | `0xF000` | 4 KB |
| ota_0 factory app | `0x10000` | 1600 KB |
| ota_1 this firmware | `0x1A0000` | 1600 KB |
| session logs | `0x330000` | 8 × 4 KB, not a partition |

Do not erase the whole chip if the factory app in `ota_0` must remain. To replace only this firmware:

```
esptool.py --chip esp32c3 -p COMx -b 460800 write_flash 0x1A0000 release/FGMDCharge_FD_ROUND_V2.bin
```

`BUILD_RELEASE.bat` builds. `GO.bat` builds and writes bootloader, partition table, factory image and this app. It asks for the COM port.

## ESP32-S3 later

This tree **does not build for ESP32-S3**. `sdkconfig` is `esp32c3`. Nothing here was run on an S3.

When the cube board is replaced by a plain ESP32-S3 module, keep the same parts (ST7789, CST816S, CAN transceiver, two relay drivers) and change the chip target:

```
idf.py set-target esp32s3
```

Then set flash size to the module you bought (4 MB is enough) and point the console at USB Serial/JTAG if you do not fit a second USB-UART.

**GPIO19 on ESP32-S3 is USB D−.** This cube uses GPIO19 as LCD SCLK, so that one wire must move. GPIO20 is USB D+ and is UART RX on the C3, so do not reuse it as a button or a display pin if you want USB. Suggested S3 wiring if you redraw the board (not tested):

| Function | C3 now | S3 suggestion |
|---|---|---|
| LCD MOSI | 5 | 11 |
| LCD SCLK | 19 | 12 |
| LCD CS | 4 | 10 |
| LCD DC | 18 | 13 |
| Backlight | 2 | 14 |
| Touch SDA | 1 | 8 |
| Touch SCL | 8 | 9 |
| CAN TX | 10 | 17 |
| CAN RX | 9 | 18 |
| Relay A | 6 | 15 |
| Relay B | 7 | 16 |
| Console | UART0 GPIO21/20 | native USB GPIO19/20, leave them free |

Avoid S3 strap pins GPIO0, GPIO3, GPIO45 and GPIO46. Edit only `main/config.h` for the new numbers, then build. CAN, the GB/T state machine and the website do not care which GPIO is which, as long as `config.h` matches the wires.

## Layout

- `main/` — CAN, GB/T 2011 / 2015 / 2023-A / 2023-B, display, Wi-Fi, site, OTA, session log
- `original/` — factory dump, not MIT
- `docs/` — extra notes
- `host_tests/` — protocol checks with no chip
- `RELEASE_NOTES_RU.md` — what changed in each release

## Русский

Публичная прошивка круглого куба на ESP32-C3. На CAN станции постоянного тока куб отвечает как BMS автомобиля, а не как зарядная станция.

Релиз **2.8.17**. Прошивку, скрипты сборки и эту документацию написал **Grok (xAI)**. Плата и заводской образ уже были в кубе. Плату Grok не разрабатывал и заводскую прошивку не писал.

### Кто производитель

**Неизвестно.**

Заводской образ считан с куба. В нём читаются строки:

| Где | Текст |
|---|---|
| Имя проекта ESP-IDF | `FD_ROUND` |
| Надпись на экране | `EVCHARGER` |
| Имя BLE в образе | `FGMDCharge` |
| Чип | ESP32-C3 |
| Строка в журнале заводской прошивки | ILI9341 |

Названия фирмы, строки copyright и сайта в образе нет. `original/` — дамп того, что стояло в этом кубе, а не работа названного завода.

### Заводской образ

Каталог `original/` выложен, чтобы проект можно было повторить и снова прошить **СТАРАЯ**. На него **не** распространяется MIT. См. [LICENSE](LICENSE).

| Файл | Что это |
|---|---|
| `dump1.bin` | чтение чипа, 4 МБ |
| `bootloader_0x1000_0x8000.bin` | заводской загрузчик |
| `partition_table_0x8000_0x9000.bin` | заводская таблица разделов |
| `nvs_0x9000_0xD000.bin` | заводской NVS (там могут быть старые настройки того куба) |
| `otadata_0xD000_0xF000.bin` | выбор слота OTA |
| `phy_init_0xF000_0x10000.bin` | данные PHY |
| `ota0_partition_0x190000.bin` | заводское приложение, весь раздел |
| `ota0_from_dump1.bin` | то же приложение, вырезанное из дампа |
| `ota0_irom.bin`, `ota0_tail.bin` | куски этого приложения |
| `ota1_partition_0x190000.bin`, `ota1_irom.bin` | второй слот, как он был |
| `factory_lzss.bin` | сжатая копия для меню загрузки |

Для OTA файла `release/FGMDCharge_FD_ROUND_V2.bin` эти файлы не нужны. `GO.bat` пишет заводской образ обратно в `ota_0`, поэтому ему они нужны.

### Что делает прошивка

Сама зарядку как станция она не начинает. Она отвечает станции. Передача CAN включена. Протокол **авто** выбирается по первому кадру:

| Что прислала станция | Протокол |
|---|---|
| Кадр версии, адреса 0x5F / 0x25 | GB/T 27930-2023 класс B (ChaoJi) |
| CHM | GB/T 27930-2015 и 2023 класс A |
| CRM, и ничего из строк выше | GB/T 27930-2011 |

Ток заряда кодируется отрицательным, как требует GB/T. Пауза CCS сессию не кончает. Кончают CST, CEM и CSD.

Сеть **Cube**, пароль `12345678`, сайт `http://192.168.4.1/`. Код на экране, который проверяет прошивка, — `1234`. На дисплей он не выводится.

Готовый файл приложения (1 143 440 байт, SHA256 `7a6ca69a24e2f7c1fb3f312d40313c5862d78a20a244591a26b8cd08b8d263ac`): [`release/FGMDCharge_FD_ROUND_V2.bin`](https://github.com/pomsfgend/FGMDCharge-FD-ROUND-V2/raw/main/release/FGMDCharge_FD_ROUND_V2.bin).

### Из чего повторить

Логика — 3,3 В. Катушку реле с ножки GPIO не питать. Нужен транзистор или драйвер, как на плате, либо свой.

| Деталь | Здесь | Замечание |
|---|---|---|
| Модуль | ESP32-C3, flash 4 МБ | Сборка только под этот чип. ESP-IDF **5.0.9** (`idf: >=5.0.9,<5.1.0`). |
| Экран | SPI ST7789, 240×280 | Эта прошивка включает ST7789 (инверсия, BGR, сдвиг по Y = 20). В журнале заводского образа написано ILI9341. Смотрите на ту панель, которая стоит. |
| Тач | CST816S, адрес I2C `0x15` | На кубе ID был `0xB6`. |
| CAN | трансивер 3,3 В на CANH/CANL | Марка в репозитории не указана. 250 кбит/с. |
| Реле | два выхода | высокий уровень = включено. GPIO6 подготовка, GPIO7 нагрузка. |
| Подсветка | ШИМ | GPIO2, 5 кГц, по умолчанию 70%. |
| USB | внешний USB-UART на UART0 | Не родной USB. См. предупреждение ниже. |
| Софт | ESP-IDF 5.0.9, LVGL **8.3.11** | LVGL подтягивает менеджер компонентов. `components/esp_lcd_ili9341` лежит в дереве. |

### Не замыкать RX и TX

На этом ESP32-C3 консоль — UART0: **TX = GPIO21**, **RX = GPIO20**. Они идут на микросхему USB-UART. Замыкание RX на TX убивает ESP или этот преобразователь. GPIO18 и GPIO19 заняты экраном, это не USB.

### Пины (ESP32-C3, этот куб)

Номера — GPIO из `main/config.h`.

| Функция | GPIO | Куда |
|---|---|---|
| LCD MOSI | 5 | выход |
| LCD SCLK | 19 | выход |
| LCD CS | 4 | выход |
| LCD DC | 18 | выход |
| Подсветка, ШИМ | 2 | выход |
| Сброс LCD | не подключён (`-1`) | |
| LCD MISO | нет | |
| Тач SDA | 1 | I2C |
| Тач SCL | 8 | I2C |
| CAN TX | 10 | на TXD трансивера |
| CAN RX | 9 | с RXD трансивера |
| Реле A | 6 | выход, подготовка |
| Реле B | 7 | выход, нагрузка |
| UART0 TX | 21 | консоль, значение IDF по умолчанию |
| UART0 RX | 20 | консоль, значение IDF по умолчанию |

SPI 40 МГц. CAN 250 кбит/с, расширенные кадры, фильтр принимает все.

### Карта flash (4 МБ)

| Имя | Адрес | Размер |
|---|---|---|
| загрузчик | `0x1000` | |
| таблица разделов | `0x8000` | |
| nvs | `0x9000` | 16 КБ |
| otadata | `0xD000` | 8 КБ |
| phy_init | `0xF000` | 4 КБ |
| ota_0 завод | `0x10000` | 1600 КБ |
| ota_1 эта прошивка | `0x1A0000` | 1600 КБ |
| журналы сессий | `0x330000` | 8 × 4 КБ, это не раздел |

Весь чип не стирать, если заводское приложение в `ota_0` нужно оставить. Заменить только эту прошивку:

```
esptool.py --chip esp32c3 -p COMx -b 460800 write_flash 0x1A0000 release/FGMDCharge_FD_ROUND_V2.bin
```

`BUILD_RELEASE.bat` собирает. `GO.bat` собирает и пишет загрузчик, таблицу, заводской образ и это приложение. Порт скрипт спрашивает сам.

### Позже на ESP32-S3

Это дерево **под ESP32-S3 не собирается**. `sdkconfig` — `esp32c3`. На S3 ничего отсюда не запускалось.

Когда вместо платы куба будет обычный модуль ESP32-S3, детали те же (ST7789, CST816S, трансивер CAN, два драйвера реле). Меняется чип:

```
idf.py set-target esp32s3
```

Дальше размер flash под купленный модуль (4 МБ хватает) и консоль на USB Serial/JTAG, если отдельный USB-UART не ставите.

**GPIO19 на ESP32-S3 — это USB D−.** На кубе GPIO19 — это такт экрана, этот провод придётся перенести. GPIO20 — USB D+, на C3 это ещё и RX консоли. Не занимайте его кнопкой или экраном, если нужен USB. Разводка S3, если плата рисуется заново (не проверялась):

| Функция | Сейчас C3 | Вариант S3 |
|---|---|---|
| LCD MOSI | 5 | 11 |
| LCD SCLK | 19 | 12 |
| LCD CS | 4 | 10 |
| LCD DC | 18 | 13 |
| Подсветка | 2 | 14 |
| Тач SDA | 1 | 8 |
| Тач SCL | 8 | 9 |
| CAN TX | 10 | 17 |
| CAN RX | 9 | 18 |
| Реле A | 6 | 15 |
| Реле B | 7 | 16 |
| Консоль | UART0 GPIO21/20 | родной USB GPIO19/20, не занимать |

Не трогайте strap-пины S3: GPIO0, GPIO3, GPIO45, GPIO46. Новые номера пишутся только в `main/config.h`, затем сборка. CAN, автомат GB/T и сайт не зависят от номеров, если `config.h` совпадает с проводами.

### Состав дерева

- `main/` — CAN, GB/T 2011 / 2015 / 2023-A / 2023-B, экран, Wi-Fi, сайт, OTA, журнал
- `original/` — заводской дамп, не MIT
- `docs/` — дополнительные заметки
- `host_tests/` — проверки протокола без чипа
- `RELEASE_NOTES_RU.md` — что менялось в релизах
