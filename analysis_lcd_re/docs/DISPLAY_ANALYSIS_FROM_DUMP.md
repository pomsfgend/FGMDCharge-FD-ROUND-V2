# Анализ дисплея из оригинального dump1.bin

**Дата:** 2026-10-07  
**Источник:** app partition OTA0 @ file offset 0x10000 (и зеркало OTA1)

## 1. Что подтверждено по строкам и API в бинарнике

### Драйвер и стек
- **Контроллер в коде:** ILI9341  
  - Файл исходника в образе: `./main/esp_lcd_ili9341.c` / `/./main/esp_lcd_ili9341.c`
  - Вызовы:
    - `esp_lcd_new_panel_ili9341`
    - `esp_lcd_new_panel_io_spi`
    - `spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO)`
    - `esp_lcd_panel_reset` / `esp_lcd_panel_init` / `esp_lcd_panel_mirror(..., false, false)` / `esp_lcd_panel_disp_on_off(..., true)`
    - `esp_lcd_panel_io_tx_param` / `esp_lcd_panel_io_tx_color`
    - ops: `panel_ili9341_init`, `panel_ili9341_reset`, `panel_ili9341_draw_bitmap`, `panel_ili9341_mirror`, `panel_ili9341_swap_xy`, `panel_ili9341_invert_color`, `panel_ili9341_disp_on_off`
- **UI:** LVGL  
  - `Initialize LVGL library`
  - `Register display driver to LVGL`
  - `Install LVGL tick timer` → `lvgl_tick` + `esp_timer_start_periodic(..., EXAMPLE_LVGL_TICK_PERIOD_MS * 1000)`
  - `Display LVGL Meter Widget`
  - буферы `buf1` / `buf2`
- **Задача:** `myTask_lcd` (рядом с `myTask_CAN`, `myTask_Timer`, `myTask_BLE`)
- **Точка входа инициализации:** `lcd_main`

### Что НЕ найдено в строках
- Нет строк `ST7789` / `st7789`
- Нет `CST816` / `cst816` (адрес 0x15 и I2C пины подтверждены в других проходах по runtime/ADC/I2C, не по имени контроллера)
- Нет `EXAMPLE_PIN_NUM_*`, `MOSI_GPIO`, `BL_GPIO` и т.п. — пины зашиты числами без имён
- Нет явных строк разрешения «240x280» / «240x320»

## 2. Пины SPI / LCD — статус

В предыдущих проходах RE была зафиксирована карта:

| Сигнал | GPIO (предыдущий вывод) | Confidence в старых отчётах |
|--------|-------------------------|-----------------------------|
| MOSI   | 5                       | CONFIRMED (по старым xref) |
| SCLK   | 19                      | CONFIRMED |
| CS     | 4                       | CONFIRMED |
| DC     | 18                      | CONFIRMED |
| MISO   | -1                      | CONFIRMED |
| RST    | -1                      | CONFIRMED (ошибка «configure GPIO for RST line failed» есть, но конфиг с -1) |
| BL     | -1                      | CONFIRMED (в оригинале подсветка не управляется GPIO) |

**Важно:** пользователь сообщает, что именно эта комбинация вызывает **bootloop**.  
Возможные причины:

1. **Ошибка извлечения пинов** — в текущей среде полный RISC-V xref/сегментный анализ ограничен; константы пинов не лежат в очевидных структурах rodata и не всплывают как простые immediate-кластеры. Предыдущий вывод мог опираться на частичные данные / пример ESP-IDF / динамику.
2. **Конфликт со strapping / другой периферией** на реальной плате (GPIO2/8/9 — strapping; GPIO8 уже SCL touch; GPIO9 — CAN RX).
3. **Аппаратная разводка платы ≠ той, под которую собрана прошивка в dump1.bin**.
4. SPI host / DMA / частота в реконструкции отличаются от оригинала.

**Рекомендация:**  
- Не считать пины 5/19/4/18 окончательными без прозвонки PCB (логическим анализатором / мультиметром от контроллера дисплея к ESP32-C3).  
- В реконструкции оставить `RST = -1`, `BL = -1` (или вынести BL на отдельный безопасный GPIO / PWM только после проверки).  
- GPIO3 (LEDC ~60 кГц) в оригинале **не** является backlight (в старых отчётах это отдельно подтверждено).

## 3. Частота SPI и прочее

- В логах: `Initialize SPI bus`, `spi_bus_config_t`, `LCD_HOST`.
- Константа 20 MHz / 10 MHz в чистом виде в IROM/DROM **не найдена** простым поиском (возможно, вычисляется или лежит в другом виде).
- Mode 0, DMA auto — по API-вызовам.

## 4. Последовательность инициализации (из логов в бинарнике)

```
Initialize SPI bus
spi_bus_config_t
spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO)
Install panel IO
esp_lcd_new_panel_io_spi(..., &io_config, &io_handle)
Install ILI9341 panel driver
esp_lcd_new_panel_ili9341(...)
esp_lcd_panel_reset
esp_lcd_panel_init
esp_lcd_panel_mirror(false, false)
esp_lcd_panel_disp_on_off(true)
Initialize LVGL library
Register display driver to LVGL
Install LVGL tick timer
Display LVGL Meter Widget
```

## 5. Что делать для воссоздания

1. **Прозвонить** MOSI/SCLK/CS/DC/RST/BL на плате от разъёма/контроллера дисплея к ножкам ESP32-C3.  
2. В коде использовать `esp_lcd` + ILI9341 (как в дампе) **или** попробовать ST7789 (часто 240×280), если картинка не появляется.  
3. `reset_gpio_num = -1`, backlight не трогать GPIO6/7 (реле).  
4. Не драйвить GPIO2.  
5. LVGL tick — значение `EXAMPLE_LVGL_TICK_PERIOD_MS` в дампе не извлечено числом; в практике реконструкций использовали 2 ms.  
6. Задача `myTask_lcd` — отдельный FreeRTOS task.

## 6. Ограничение текущего прохода

Полный дизассемблер с корректными VA→file mapping и поиском всех `li`/`addi` imm в callers `spi_bus_initialize` / `gpio_set_level` в этой среде не дал новых кластеров пинов, отличных от 5/19/4/18.  
Если появятся прозвоненные пины с платы — можно сразу сопоставить с бинарником точечно.

