# FGMDCharge / FD_ROUND V2

[English](../../README.md) · [Русский](../../README.md#русский) · [中文](README.zh-CN.md) · [Español](README.es.md) · [Português](README.pt-BR.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · 日本語 · [한국어](README.ko.md)

丸い ESP32-C3 キューブ向けの公開ファームウェア。直流充電器の CAN 上で車両 BMS として応答する。充電器そのものにはならない。

版 **2.8.17**。ピン、フラッシュ配置、詳しい説明は [README.md](../../README.md)。変更履歴は [CHANGELOG.md](../../CHANGELOG.md)。経緯は [CHRONOLOGY.md](../../CHRONOLOGY.md)。

このツリーは、`original/` の工場ダンプと実機の充電器での試験から、長い時間をかけてバイブコーディングしたもの。単独の作者はいない。工場イメージはここで書かれたものではなく、MIT でもない。基板メーカーは不明。イメージ内の文字列は `FD_ROUND` と画面タイトル `EVCHARGER` だけ。

## ダウンロード

| ファイル | 用途 |
|---|---|
| [2.8.17 の bin](../../release/FGMDCharge_FD_ROUND_V2.bin) | 現行アプリ。`ota_1` の `0x1A0000` に書く |
| [2.8.16 の一式](../../archive/2.8.16/) | 一つ前。ログは softAP started でも電話に Cube が出ない |
| [工場ダンプ](../../original/) | 4 MB の読み出し。このリポジトリのソースではない |

`ota_0` の工場イメージを残すなら、チップ全体は消さない。

```
esptool.py --chip esp32c3 -p COMx -b 460800 write_flash 0x1A0000 release/FGMDCharge_FD_ROUND_V2.bin
```

Windows で ESP-IDF 5.0.9 を入れ、`BUILD_RELEASE.bat`。対象は ESP32-C3 のみ。

## 使い方

Wi-Fi **Cube**、パスワード `12345678`、ページ `http://192.168.4.1/`。電源投入後は **НОВАЯ**。プロトコルは自動。バージョンフレームなら GB/T 27930-2023 B（ChaoJi）、CHM なら 2015 / 2023-A、CRM だけなら 2011。CAN は 250 kbit/s。

## 参加

Issue と pull request は日本語、English、中文、Русский、Español、Português、Deutsch、Français、한국어でよい。[Issues](https://github.com/pomsfgend/FGMDCharge-FD-ROUND-V2/issues)。MAC や自宅のパス、個人情報は貼らない。Cube の AP パスワードは書いてよい。
