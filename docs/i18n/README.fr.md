# FGMDCharge / FD_ROUND V2

[English](../../README.md) · [Русский](../../README.md#русский) · [中文](README.zh-CN.md) · [Español](README.es.md) · [Português](README.pt-BR.md) · [Deutsch](README.de.md) · Français · [日本語](README.ja.md) · [한국어](README.ko.md)

Firmware public du cube rond à ESP32-C3. Sur le CAN de la borne DC il répond comme BMS du véhicule. Il ne lance pas la charge à la place de la borne.

Version **2.8.17**. Broches, carte flash et texte long : [README.md](../../README.md). Journal : [CHANGELOG.md](../../CHANGELOG.md). Déroulé : [CHRONOLOGY.md](../../CHRONOLOGY.md).

L'arbre vient d'un vibe coding long, à partir du dump d'usine dans `original/` et d'essais sur de vraies bornes. Il n'y a pas un seul auteur. L'image d'usine n'a pas été écrite ici et n'est pas sous MIT. Le fabricant de la carte est inconnu : l'image ne contient que `FD_ROUND` et le titre `EVCHARGER`.

## Téléchargements

| Fichier | Rôle |
|---|---|
| [bin 2.8.17](../../release/FGMDCharge_FD_ROUND_V2.bin) | Application actuelle. Écrire dans `ota_1`, adresse `0x1A0000` |
| [paquet 2.8.16](../../archive/2.8.16/) | La précédente. Le journal disait softAP, le téléphone ne voyait pas Cube |
| [dump d'usine](../../original/) | Lecture de 4 Mo. Pas le code de ce dépôt |

Ne pas effacer toute la puce si l'image d'usine dans `ota_0` doit rester.

```
esptool.py --chip esp32c3 -p COMx -b 460800 write_flash 0x1A0000 release/FGMDCharge_FD_ROUND_V2.bin
```

Compiler sous Windows avec ESP-IDF 5.0.9 : `BUILD_RELEASE.bat`. ESP32-C3 seulement.

## Utilisation

Wi-Fi **Cube**, mot de passe `12345678`, page `http://192.168.4.1/`. À la mise sous tension, **НОВАЯ**. Protocole automatique : trame de version → GB/T 27930-2023 B (ChaoJi), CHM → 2015 / 2023-A, CRM seul → 2011. CAN à 250 kbit/s.

## Participer

Issues et pull requests en français, English, 中文, Русский, Español, Português, Deutsch, 日本語 ou 한국어. [Issues](https://github.com/pomsfgend/FGMDCharge-FD-ROUND-V2/issues). Pas de MAC, pas de chemin personnel. Le mot de passe de l'AP Cube peut être cité.
