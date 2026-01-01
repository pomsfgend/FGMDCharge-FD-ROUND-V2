# Технический отчёт по dump1.bin

## 1. Идентификация

| Параметр | Значение | Уверенность |
|---|---|---|
| MCU | ESP32-C3 | высокая |
| Архитектура | RISC-V | высокая |
| Проект | FD_ROUND | высокая |
| App version | 1 | высокая |
| ESP-IDF | v5.0.9 | высокая |
| Build | Dec 29 2025 14:57:44 | высокая |
| Flash dump | 4 MiB | высокая |
| OTA copies | 2 | высокая |

SHA256 дампа:
`1428dbddc0749eee8ac133b1e16b6c0805a2da4bb4b0c73bc759c1500b03f233`

App ELF descriptor SHA256:
`9506525a360122b5ff3d32b63186176df5777d9ec91151bf37ad2c09d0d539d3`

## 2. Таблица разделов

```text
nvs       0x00009000  0x00004000
otadata   0x0000D000  0x00002000
phy_init  0x0000F000  0x00001000
ota_0     0x00010000  0x00190000
ota_1     0x001A0000  0x00190000
```

Обе OTA-копии содержат одинаковое приложение.

## 3. App image

6 сегментов:

```text
seg0 DROM  0x3C0F0020  len 0x455B8
seg1 DRAM  0x3FC95400  len 0x2118
seg2 IRAM  0x40380000  len 0x8918
seg3 IROM  0x42000020  len 0xE2208
seg4 IRAM  0x40388918  len 0xC910
seg5 RTC   0x50000010  len 0x4
```

Entry: `0x40380434`.

Важное техническое замечание: код приложения находится также в XIP/IROM-области `0x42000020`; при дизассемблировании нельзя анализировать только IRAM-сегменты.

## 4. Исходная структура проекта

В бинарнике сохранились имена файлов:

```text
./main/esp_lcd_ili9341.c
./main/BLE.c
./main/time.c
./main/CAN.c
./main/ADC.c
```

Сохранились имена основных функций:

```text
lcd_main
BLE_init
gatts_profile_event_handler
Time_int
CAN1Process_Mails_Receive
Can_Send_Msg
CAN_Mode_Init
adc_loop
adc_init
pwm_init
```

Это очень сильная подсказка для реконструкции.

## 5. FreeRTOS-архитектура приложения

`app_main` находится около `0x42007D62`.

Создаются четыре задачи:

| Задача | Entry | Stack | Priority |
|---|---:|---:|---:|
| myTask_BLE | `0x42007D4E` | 4096 | 3 |
| myTask_CAN | `0x42007D2E` | 4096 | 4 |
| myTask_lcd | `0x42007D08` | 4096 | 2 |
| myTask_Timer | `0x42007D20` | 4096 | 2 |

`myTask_CAN` имеет приоритет выше UI/BLE.

## 6. TFT / ILI9341

Подтверждено использование ESP-IDF `esp_lcd` и драйвера `ILI9341`.

Параметры SPI:

```c
MOSI = GPIO5
MISO = -1
SCLK = GPIO19
CS   = GPIO4
DC   = GPIO18
SPI mode = 0
PCLK = 20,000,000 Hz
trans_queue_depth = 10
max_transfer_sz = 0x9600 = 38400 bytes
LCD command bits = 8
LCD parameter bits = 8
```

Вызовы явно присутствуют:

```text
spi_bus_initialize
esp_lcd_new_panel_io_spi
esp_lcd_new_panel_ili9341
esp_lcd_panel_reset
esp_lcd_panel_init
esp_lcd_panel_mirror
esp_lcd_panel_disp_on_off
```

Используется LVGL:

```text
Initialize LVGL library
Register display driver to LVGL
Install LVGL tick timer
Display LVGL Meter Widget
```

Важный вывод: внешний вид экрана не нужно патчить в BIN. Его разумнее пересобрать в LVGL-сцене нового проекта.

Touch-контроллер по текущему дампу не идентифицирован. Не считать сенсорный ввод подтвержденным только потому, что на модуле есть touchscreen.

## 7. CAN / TWAI

Конфигурация TWAI:

```c
mode = normal
TX = GPIO10
RX = GPIO9
TX queue = 10
RX queue = 10
alerts = 0

BRP = 16
TSEG1 = 15
TSEG2 = 4
SJW = 3
triple sampling = 0

acceptance code = 0
acceptance mask = 0xFFFFFFFF
single filter = 1
```

При APB 80 MHz расчёт даёт 250 kbit/s.

Подтвержденные вызовы:

```text
twai_driver_install
twai_start
twai_transmit(&TxMessage, portMAX_DELAY)
twai_receive(&RxMessage, portMAX_DELAY)
```

Таким образом для собственного проекта базовая CAN-конфигурация известна.

## 8. Обработанные CAN ID

Парсер `CAN1Process_Mails_Receive` проверяет сообщения с окончанием `F456` и содержит следующие точные ID:

```text
0x1807F456
0x1801F456
0x1826F456
0x100AF456
0x101AF456
0x181DF456
0x081FF456
0x1808F456
0x1812F456
```

Имена сообщений нельзя автоматически считать стандартными названиями только по ID: в старой прошивке разработчик мог использовать собственные метки. Поэтому в новой прошивке эти ID пока фиксируем как сырые идентификаторы, а семантику подтверждаем тестом CAN.

## 9. Частично восстановленная структура данных CAN

Парсер пишет состояние в RAM около `0x3FC98200`.

Подтверждены поля/смещения:

```text
+0x08  16-bit parsed value
+0x0A  16-bit parsed value
+0x0C  16-bit parsed value
+0x0E  16-bit parsed value
+0x10  16-bit parsed value
+0x12  16-bit parsed value
+0x18  message timestamp/counter
+0x1C  message timestamp/counter
+0x20  message timestamp/counter
+0x24  message timestamp/counter
+0x28  message timestamp/counter
+0x2C  message timestamp/counter
+0x30  message timestamp/counter
+0x34  message timestamp/counter
+0x38  message timestamp/counter
+0x3C  byte state/value
+0x3E  byte state/value
```

Для ряда 16-bit полей видно преобразование вида `raw - 0x190` (400), а для отображаемых токовых/связанных значений применяется дополнительное масштабирование/смена знака. Это нужно переносить после привязки каждого поля к конкретному GB/T сообщению.

## 10. BLE

Сохранились:

```text
BLE_init
gatts_profile_event_handler
GATTS_TABLE_DEMO
BLEname
Alldata
```

Имя из NVS: `FGMDCharge`.

Есть OTA через BLE:

```text
ota-control
beginota
esp_ota_begin
esp_ota_write
esp_ota_end
esp_ota_set_boot_partition
```

BLE-код похож на модифицированный Espressif GATTS table demo.

## 11. NVS

Найдены ключи:

```text
BLEname
Alldata
```

Также присутствуют служебные записи Bluetooth/NVS.

Секретные Bluetooth-ключи обнаружены в NVS. В новый проект их не нужно переносить вручную; оставляем процедуру генерации/инициализации стандартной для нового устройства.

## 12. ADC/PWM

Найдены исходный файл `./main/ADC.c` и вызовы:

```text
ledc_timer_config
ledc_channel_config
adc_oneshot_new_unit
adc_oneshot_config_channel
adc_oneshot_read
adc_cali_raw_to_voltage
ledc_set_duty
ledc_update_duty
```

Из текущего статического анализа безопасно подтверждены наличие ADC1 + oneshot + calibration + LEDC/PWM, но точная физическая привязка ADC/GPIO и окончательные коэффициенты измерения ещё требуют отдельной реконструкции.

Нельзя использовать догаданные GPIO в готовой прошивке.

## 13. Оригинальный интерфейс

В бинарнике присутствуют UI-строки:

```text
EVCHARGER
50~500V
需要电压
需要电流
输出电压
输出电流
功率
状态
时间:
输出:
请输入6位数字密码...
OFF
0.0
min
```

То есть оригинальный UI включает напряжение/ток/мощность/состояние и парольный ввод.

## 14. Хвост ota_0

После валидного app image в `ota_0` присутствует дополнительная область примерно 79 KiB, отсутствующая в `ota_1`.

Она не распознаётся как PNG/JPEG/ELF/ZIP и имеет высокую энтропию.

Она сохранена отдельно как `ota0_tail.bin`.

Пока считать её неизвестным/остаточным blob и не включать автоматически в новую прошивку.

## 15. План новой прошивки

### Уровень 1 — аппаратный слой

- `board_pins.h`
- ILI9341
- TWAI
- BLE
- Wi-Fi
- ADC/LEDC

### Уровень 2 — протокол

- `gbt_protocol.c/.h`
- RX parser
- TX scheduler
- state machine зарядки

### Уровень 3 — данные

Единый runtime state:

```text
charger voltage
charger current
requested voltage
requested current
power
charging state
elapsed time
CAN health
BLE health
Wi-Fi health
```

### Уровень 4 — UI

LVGL страницы:

1. Dashboard
2. Charging
3. CAN
4. Settings
5. System

### Уровень 5 — Web UI

Wi-Fi AP + HTTP server.

План URL:

```text
/
/api/status
/api/settings
/api/can/recent
/api/can/send
/api/system
```

На раннем этапе `/api/can/send` должен быть защищён и отключён по умолчанию.

## 16. Безопасный порядок тестирования

1. Сначала собрать проект без новой силовой логики.
2. Проверить загрузку ESP32-C3.
3. Проверить UART/log.
4. Проверить TFT.
5. Проверить CAN только на пассивном приёме.
6. Проверить BLE.
7. Проверить Wi-Fi/web.
8. Только потом включать активную передачу GB/T.
9. Только после записи нескольких реальных сеансов зарядки переносить полную state machine.

## 17. Что уже можно считать готовым для реализации

- точный MCU;
- точная версия ESP-IDF;
- разделы flash;
- SPI TFT pinout;
- CAN pinout и скорость;
- существование LVGL;
- структура FreeRTOS задач;
- BLE имя;
- наличие BLE OTA;
- список CAN ID;
- архитектура исходного приложения.

## 18. Что пока нельзя честно объявлять восстановленным

- исходный `.c/.h` код 1:1;
- точные UUID всех пользовательских BLE characteristic без окончательного декодирования GATT DB;
- точные ADC GPIO/коэффициенты;
- точная семантика каждого byte/word CAN сообщения;
- touch controller;
- назначение дополнительного 79 KiB blob;
- физическая распиновка всего кубика без фото платы/трассировки.

Это разделено намеренно, чтобы в новую прошивку не попали догадки.
