# FGMDCharge / FD_ROUND V2

[English](../../README.md) · [Русский](../../README.md#русский) · [中文](README.zh-CN.md) · [Español](README.es.md) · [Português](README.pt-BR.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [日本語](README.ja.md) · 한국어

둥근 ESP32-C3 큐브의 공개 펌웨어. 직류 충전기 CAN에서 차량 BMS로 응답한다. 충전기 역할로 충전을 시작하지 않는다.

버전 **2.8.17**. 핀, 플래시 맵, 긴 설명은 [README.md](../../README.md). 변경 기록은 [CHANGELOG.md](../../CHANGELOG.md). 작업 순서는 [CHRONOLOGY.md](../../CHRONOLOGY.md).

이 트리는 `original/`의 공장 덤프와 실제 충전기 시험으로 오래 바이브 코딩한 결과다. 단독 저자는 없다. 공장 이미지는 여기서 쓴 것이 아니고 MIT도 아니다. 기판 제조사는 알 수 없다. 이미지 안의 문자열은 `FD_ROUND`와 화면 제목 `EVCHARGER`뿐이다.

## 다운로드

| 파일 | 용도 |
|---|---|
| [2.8.17 bin](../../release/FGMDCharge_FD_ROUND_V2.bin) | 현재 앱. `ota_1`, 주소 `0x1A0000`에 기록 |
| [2.8.16 묶음](../../archive/2.8.16/) | 이전 판. 로그는 softAP인데 전화에 Cube가 안 보임 |
| [공장 덤프](../../original/) | 4 MB 읽기. 이 저장소의 소스가 아님 |

`ota_0`의 공장 이미지를 남기려면 칩 전체를 지우지 않는다.

```
esptool.py --chip esp32c3 -p COMx -b 460800 write_flash 0x1A0000 release/FGMDCharge_FD_ROUND_V2.bin
```

Windows, ESP-IDF 5.0.9에서 `BUILD_RELEASE.bat`. 대상은 ESP32-C3만.

## 사용

Wi-Fi **Cube**, 비밀번호 `12345678`, 페이지 `http://192.168.4.1/`. 전원을 켜면 **НОВАЯ**. 프로토콜은 자동. 버전 프레임이면 GB/T 27930-2023 B(ChaoJi), CHM이면 2015 / 2023-A, CRM만이면 2011. CAN 250 kbit/s.

## 참여

이슈와 풀 리퀘스트는 한국어, English, 中文, Русский, Español, Português, Deutsch, Français, 日本語로 써도 된다. [Issues](https://github.com/pomsfgend/FGMDCharge-FD-ROUND-V2/issues). MAC, 집 경로, 개인 정보는 붙이지 않는다. Cube AP 비밀번호는 적어도 된다.
