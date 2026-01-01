# FD_ROUND V2 — release 2.1.0

## Исправлено по реальному runtime 2.0.5

- LCD SPI: 20 MHz — ровно как в `lcd_main` исходного dump1.bin.
- ILI9341: `rgb_endian = RGB`.
- ILI9341 orientation: `mirror(false, false)`.
- Убраны `set_gap()` и `invert_color(true)`, которых нет в оригинальной последовательности.
- LVGL DMA buffers: 240×20 пикселей × 2, `MALLOC_CAP_DMA` — ровно восстановленный размер `0x2580` байт каждого буфера.
- SPI color-transfer callback и `user_ctx` подключены непосредственно при создании panel IO.
- LVGL `CONFIG_LV_COLOR_16_SWAP=y` сохранён; дополнительный byte-swap в flush не добавляется.
- Wi-Fi AP: WPA2-PSK/CCMP с паролем `FDcube2026`.
- Добавлен лог STA connect/disconnect и reason code.
- Добавлены captive-portal probes (`generate_204`, `gen_204`, `hotspot-detect.html`, `connecttest.txt`, `ncsi.txt`, `success.txt`).
- Web write-control key: `FGMD-CUBE-2026`; GET API остаётся открытым для локального мониторинга.
- Максимум HTTP sockets возвращён к 4 для устойчивой работы на 4 MB ESP32-C3.
- Убрано принудительное `Connection: close` с каждого HTTP ответа; добавлен GET catch-all для captive-portal probe путей.

## Не выдуманные параметры

ADC/PWM силовые GPIO по-прежнему не активируются автоматически: dump подтверждает наличие ADC oneshot + LEDC, но не даёт достаточно надёжной карты физических силовых выходов для безопасного управления зарядом.
