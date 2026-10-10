# FGMDCharge / FD_ROUND V2

[English](../../README.md) · [Русский](../../README.md#русский) · 中文 · [Español](README.es.md) · [Português](README.pt-BR.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

圆形 ESP32-C3 小方块的公开固件。它在直流充电桩的 CAN 上充当车辆 BMS，自己不充当充电桩。

当前版本 **2.8.17**。完整说明、引脚和 flash 地图在 [README.md](../../README.md)。变更记录 [CHANGELOG.md](../../CHANGELOG.md)。过程 [CHRONOLOGY.md](../../CHRONOLOGY.md)。

这套代码是长期对照出厂 dump（`original/`）和实桩试出来的 vibe coding。没有单一作者。出厂镜像不是在本仓库里写的，许可证也不是 MIT。板子厂家未知，镜像里只有项目名 `FD_ROUND`、屏幕标题 `EVCHARGER`。

## 下载

| 文件 | 用途 |
|---|---|
| [2.8.17 bin](../../release/FGMDCharge_FD_ROUND_V2.bin) | 当前应用，写入 `ota_1`，地址 `0x1A0000` |
| [2.8.16 源码包和 bin](../../archive/2.8.16/) | 上一版，Wi-Fi 灯亮但手机常常搜不到 |
| [出厂 dump](../../original/) | 4 MB 整片读出，不是本项目源码 |

不要整片擦除，如果还要保留出厂槽 `ota_0`。

```
esptool.py --chip esp32c3 -p COMx -b 460800 write_flash 0x1A0000 release/FGMDCharge_FD_ROUND_V2.bin
```

Windows 上用已安装的 ESP-IDF 5.0.9 运行 `BUILD_RELEASE.bat`。目标芯片只能是 ESP32-C3。

## 使用

Wi-Fi **Cube**，密码 `12345678`，页面 `http://192.168.4.1/`。上电选 **НОВАЯ**。协议自动：版本帧选 GB/T 27930-2023 B（ChaoJi），CHM 选 2015 / 2023-A，只有 CRM 则选 2011。CAN 250 kbit/s。

## 参与

Bug 和拉取请求可以用中文、English、Русский、Español、Português、Deutsch、Français、日本語、한국어。[Issues](https://github.com/pomsfgend/FGMDCharge-FD-ROUND-V2/issues)。请不要贴 MAC、家里的路径或串口密码以外的个人信息。Cube 的出厂热点密码可以写。
