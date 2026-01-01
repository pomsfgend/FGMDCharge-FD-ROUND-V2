#!/usr/bin/env python3
"""Force BLE + size + required LVGL fonts/img."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
p = root / "sdkconfig"
required = {
    "CONFIG_BT_ENABLED": "y",
    "CONFIG_BT_CONTROLLER_ENABLED": "y",
    "CONFIG_BT_BLUEDROID_ENABLED": "y",
    "CONFIG_BT_BLE_ENABLED": "y",
    "CONFIG_BT_GATTS_ENABLE": "y",
    "CONFIG_BT_GATTC_ENABLE": "n",
    "CONFIG_BT_BLE_42_FEATURES_SUPPORTED": "y",
    "CONFIG_BT_BLE_42_ADV_EN": "y",
    "CONFIG_BT_BLE_42_SCAN_EN": "y",
    "CONFIG_BT_BLE_42_DTM_TEST_EN": "n",
    "CONFIG_BT_BLE_50_FEATURES_SUPPORTED": "n",
    "CONFIG_BT_CLASSIC_ENABLED": "n",
    "CONFIG_BT_NIMBLE_ENABLED": "n",
    "CONFIG_COMPILER_OPTIMIZATION_SIZE": "y",
    "CONFIG_FREERTOS_USE_TRACE_FACILITY": "y",
    "CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS": "y",
    "CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER": "y",
    "CONFIG_LOG_DEFAULT_LEVEL_INFO": "y",
    "CONFIG_LV_USE_IMG": "y",
    "CONFIG_LV_USE_LABEL": "y",
    "CONFIG_LV_FONT_MONTSERRAT_14": "y",
    "CONFIG_LV_FONT_MONTSERRAT_28": "y",
}
lines = p.read_text(encoding="utf-8").splitlines() if p.exists() else []
out = []
seen = set()
for line in lines:
    key = None
    if line.startswith("CONFIG_"):
        key = line.split("=", 1)[0]
    elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
        key = line[2:].split(" ", 1)[0]
    if key in required:
        out.append(f"{key}={required[key]}")
        seen.add(key)
    else:
        out.append(line)
for key, value in required.items():
    if key not in seen:
        out.append(f"{key}={value}")
p.write_text("\n".join(out) + "\n", encoding="utf-8")
print("sdkconfig policy forced: Bluedroid + size + LV fonts/img")
