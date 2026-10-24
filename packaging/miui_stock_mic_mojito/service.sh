#!/system/bin/sh
# ========================================================
# MIUI Stock Mic & Studio Call Quality - Safe Initializer
# ========================================================
MODDIR=${0%/*}

# Safe Boot Completed Watchdog: Poll up to 45s max
TIMEOUT=45
while [ "$TIMEOUT" -gt 0 ] && [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 2
    TIMEOUT=$((TIMEOUT - 2))
done

# Wait for audioserver to fully stabilize
sleep 3

# Inject Qualcomm Fluence Pro dual-mic hardware noise suppression properties
resetprop ro.vendor.audio.sdk.fluencetype fluencepro
resetprop persist.vendor.audio.fluence.voicecall true
resetprop persist.vendor.audio.fluence.voicerec true
resetprop persist.vendor.audio.fluence.speaker true
resetprop persist.vendor.audio.fluence.audiorec true
resetprop vendor.audio.sdk.fluencetype fluencepro
resetprop vendor.audio.feature.fluence.enable true
resetprop persist.vendor.audio.spv3.enable true

# Hardware Anti-Hum Filter: Cut off 50Hz/60Hz AC charger switching ripple and DC bias
tinymix "TX0 HPF cut off" "CF_NEG_3DB_75HZ" 2>/dev/null || true
tinymix "TX1 HPF cut off" "CF_NEG_3DB_75HZ" 2>/dev/null || true

# Balanced Voice & Ambient Gain Stages (Official MIUI Stock Profile)
tinymix "TX0 Digital Volume" 84 2>/dev/null || true
tinymix "TX1 Digital Volume" 84 2>/dev/null || true

exit 0
