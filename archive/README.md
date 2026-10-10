# archive/

Packages that still exist as files. A version that is not listed here was not kept. Its text is in [CHANGELOG.md](../CHANGELOG.md) under `notes/…`. Do not flash a `notes/` zip from GitHub: that zip is today's tree, not the old firmware.

Пакеты, которые реально остались файлами. Номера, которых здесь нет, не сохранились. Текст о них — в [CHANGELOG.md](../CHANGELOG.md), метки `notes/…`. Архив с метки `notes/` прошивать нельзя: это сегодняшнее дерево, не старая прошивка.

| Version | What is in the folder | Flash? |
|---|---|---|
| 2.8.17 | Also at [release/FGMDCharge_FD_ROUND_V2.bin](../release/FGMDCharge_FD_ROUND_V2.bin). This folder has the source zip from the build that produced that bin. | Yes. `ota_1` at `0x1A0000`. This is `main`. |
| 2.8.16 | Source zip and app image. `config.h` in the zip says 2.8.16. The log said softAP started and phones often did not list Cube. Fixed in 2.8.17. | Only if you mean to run 2.8.16. |

Not here, and not found in any folder named cube files on the machine that published this:

- `analysis_original/` (disassembly listed in the 2.1.1 manifest)
- source trees 2.3.0 through 2.8.15
- the 2.8.6 zip named in an old note

The factory chip read is [original/](../original/), not in this folder. It is not MIT.

First batch of the historical zips is in [zips/](zips/CATALOG.md). It is not complete.
