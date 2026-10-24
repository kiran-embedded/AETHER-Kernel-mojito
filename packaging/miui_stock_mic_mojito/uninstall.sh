#!/system/bin/sh
# ========================================================
# MIUI Stock Mic & Studio Call Quality - Clean Uninstaller
# Reverts all system properties back to ROM defaults and
# completely purges any module remnants.
# ========================================================
MODDIR=${0%/*}

# 1. Revert all injected Qualcomm Fluence Pro properties
resetprop -p persist.vendor.audio.fluence.voicecall "" 2>/dev/null || true
resetprop -p persist.vendor.audio.fluence.voicerec "" 2>/dev/null || true
resetprop -p persist.vendor.audio.fluence.speaker "" 2>/dev/null || true
resetprop -p persist.vendor.audio.fluence.audiorec "" 2>/dev/null || true
resetprop -p ro.vendor.audio.sdk.fluencetype "" 2>/dev/null || true
resetprop -p vendor.audio.sdk.fluencetype "" 2>/dev/null || true
resetprop -p vendor.audio.feature.fluence.enable "" 2>/dev/null || true
resetprop -p persist.vendor.audio.spv3.enable "" 2>/dev/null || true

# 2. Clean up any temporary or update directory remnants
rm -rf /data/adb/modules/miui_stock_mic_mojito 2>/dev/null || true
rm -rf /data/adb/modules_update/miui_stock_mic_mojito 2>/dev/null || true

# Note: KernelSU / Magisk overlayfs automatically restores all ROM vendor
# ACDB, mixer_paths, and audio HAL files immediately upon unmount.
exit 0
