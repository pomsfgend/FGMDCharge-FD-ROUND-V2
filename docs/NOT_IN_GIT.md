# What is not in git

`original/*.bin` is a backup of the cube as it shipped. That image is not source code of this project and is not under the MIT license. It is not published here.

The firmware this repository builds is published as a release asset, `FGMDCharge_FD_ROUND_V2.bin`, on the GitHub release. `build/`, `managed_components/` and `sdkconfig` are generated locally by ESP-IDF 5.0.9.

Cable flash of both slots (`GO.bat`) needs the factory slices on your own machine. OTA of the release bin does not.
