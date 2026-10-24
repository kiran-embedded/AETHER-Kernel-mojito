#!/system/bin/sh
# ========================================================
# AETHER SuSFS Stock Spoofer - Background Service Engine
# Complete Full-Level Hardware & OS Identity Cloaking
# Certified for Xiaomi Redmi Note 10 (mojito / sunny)
# ========================================================
MODDIR=${0%/*}

# If module is marked for deletion, trigger clean uninstaller and exit immediately
if [ -f "$MODDIR/remove" ]; then
    [ -f "$MODDIR/uninstall.sh" ] && sh "$MODDIR/uninstall.sh" 2>/dev/null
    exit 0
fi

# Locate ksu_susfs binary
SUSFS_BIN="/data/adb/ksu/bin/ksu_susfs"
[ ! -x "$SUSFS_BIN" ] && SUSFS_BIN="$MODDIR/tools/ksu_susfs_arm64"

# Safe Boot Completed Watchdog: Poll up to 60s max to prevent indefinite background hang
TIMEOUT=60
while [ "$TIMEOUT" -gt 0 ] && [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 2
    TIMEOUT=$((TIMEOUT - 2))
done

# If module was deleted during boot, exit cleanly
if [ -f "$MODDIR/remove" ]; then
    [ -f "$MODDIR/uninstall.sh" ] && sh "$MODDIR/uninstall.sh" 2>/dev/null
    exit 0
fi

# Wait 5 seconds after boot completed for SystemUI, HALs, and Android framework to settle
sleep 5

# ========================================================
# Phase 1: Re-enforce SuSFS Isolation & Kernel Uname Spoofing
# ========================================================
if [ -x "$SUSFS_BIN" ]; then
    # Safely spoof kernel release & version to stock Qualcomm Xiaomi baseline
    # (bpfloader & netd are protected in-kernel and will not fail)
    "$SUSFS_BIN" set_uname "4.14.190-perf+" "#1 SMP PREEMPT Thu Jun 17 02:40:22 UTC 2021" >/dev/null 2>&1
fi

# ========================================================
# Phase 2: Hardware Bootloader & Integrity Cloaking
# Bypasses Google Play Integrity (MEETS_DEVICE_INTEGRITY),
# Paytm, PhonePe, GPay, and Enterprise Work Profiles
# ========================================================

# 1. Bootloader lock state emulation
resetprop -n ro.boot.verifiedbootstate "green"
resetprop -n ro.boot.flash.locked "1"
resetprop -n ro.boot.veritymode "enforcing"
resetprop -n ro.boot.warranty_bit "0"
resetprop -n ro.warranty_bit "0"
resetprop -n ro.debuggable "0"
resetprop -n ro.secure "1"

# 2. Release-keys & User build flavor (hides eng/userdebug tags)
resetprop -n ro.build.type "user"
resetprop -n ro.build.tags "release-keys"
resetprop -n ro.system.build.type "user"
resetprop -n ro.system.build.tags "release-keys"
resetprop -n ro.vendor.build.type "user"
resetprop -n ro.vendor.build.tags "release-keys"
resetprop -n ro.product.build.type "user"
resetprop -n ro.product.build.tags "release-keys"
resetprop -n ro.system_ext.build.type "user"
resetprop -n ro.system_ext.build.tags "release-keys"
resetprop -n ro.odm.build.type "user"
resetprop -n ro.odm.build.tags "release-keys"

# 3. Dynamic Device Identity: Support both mojito (Global) and sunny (India)
# IMPORTANT: If PlayIntegrityFix (pif.json) or TrickyStore is active, do NOT overwrite
# ro.product.* with Redmi props, because a mismatch between fingerprint and model causes
# Google Play Integrity and GPay to FAIL!
HAS_PIF=0
if [ -f "/data/adb/pif.json" ] || \
   [ -f "/data/adb/modules/playintegrityfix/pif.json" ] || \
   [ -d "/data/adb/modules/playintegrityfix" ] || \
   [ -d "/data/adb/modules/tricky_store" ] || \
   [ -f "/data/adb/modules/playintegrityfix/custom.pif.json" ]; then
    HAS_PIF=1
fi

CURRENT_FINGERPRINT=$(getprop ro.build.fingerprint 2>/dev/null)
case "$CURRENT_FINGERPRINT" in
    *Xiaomi*|*Redmi*|*POCO*|*mojito*|*sunny*)
        # Normal Xiaomi / Redmi fingerprint
        ;;
    "")
        ;;
    *)
        # Fingerprint belongs to a non-Xiaomi device (e.g. Google Pixel / Motorola from PIF)
        HAS_PIF=1
        ;;
esac

if [ "$HAS_PIF" -eq 0 ]; then
    CURRENT_DEV=$(getprop ro.product.device 2>/dev/null)
    [ -z "$CURRENT_DEV" ] && CURRENT_DEV=$(getprop ro.build.product 2>/dev/null)

    if [ "$CURRENT_DEV" = "sunny" ]; then
        TARGET_DEV="sunny"
        TARGET_MODEL="M2101K7AI"
        TARGET_NAME="sunny_in"
    else
        TARGET_DEV="mojito"
        TARGET_MODEL="M2101K7AG"
        TARGET_NAME="mojito_global"
    fi

    for part in "" ".system" ".vendor" ".product" ".system_ext" ".odm"; do
        resetprop -n "ro.product${part}.brand" "Redmi"
        resetprop -n "ro.product${part}.manufacturer" "Xiaomi"
        resetprop -n "ro.product${part}.device" "$TARGET_DEV"
        resetprop -n "ro.product${part}.name" "$TARGET_NAME"
        resetprop -n "ro.product${part}.model" "$TARGET_MODEL"
    done

    resetprop -n ro.build.flavor "${TARGET_NAME}-user"
    resetprop -n ro.build.product "$TARGET_DEV"
fi

# 4. Sanitize Build Host & User (Eliminates GitHub Actions / Custom Server Fingerprints)
resetprop -n ro.build.user "builder"
resetprop -n ro.build.host "mi-server"

# 5. Stock Play Integrity Profile Generator (pif.json)
# If no custom pif.json exists, auto-deploy certified stock Redmi Note 10 profile
if [ ! -f "/data/adb/pif.json" ]; then
    cat << 'EOF' > /data/adb/pif.json
{
  "PRODUCT": "mojito_global",
  "DEVICE": "mojito",
  "MANUFACTURER": "Xiaomi",
  "BRAND": "Redmi",
  "MODEL": "M2101K7AG",
  "FINGERPRINT": "Redmi/mojito_global/mojito:11/RKQ1.201022.002/V12.5.4.0.RKGMIXM:user/release-keys",
  "SECURITY_PATCH": "2021-10-01",
  "FIRST_API_LEVEL": "25"
}
EOF
    chmod 644 /data/adb/pif.json
fi

# Also sync to PlayIntegrityFix module folder if present
if [ -d "/data/adb/modules/playintegrityfix" ] && [ ! -f "/data/adb/modules/playintegrityfix/pif.json" ]; then
    cp -f /data/adb/pif.json /data/adb/modules/playintegrityfix/pif.json
fi

# 6. Ensure TrickyStore target.txt has GMS and Google Wallet protected
if [ -d "/data/adb/tricky_store" ]; then
    if [ ! -f "/data/adb/tricky_store/target.txt" ]; then
        echo "com.google.android.gms" > /data/adb/tricky_store/target.txt
        echo "com.android.vending" >> /data/adb/tricky_store/target.txt
        echo "com.google.android.apps.walletnfcrel" >> /data/adb/tricky_store/target.txt
        chmod 644 /data/adb/tricky_store/target.txt
    fi
fi

# 7. Flush Google Play Services Isolated Attestation Cache
# Remove stale DroidGuard database so GMS cannot reuse old failed attestation
rm -f /data/data/com.google.android.gms/databases/dg.db* >/dev/null 2>&1
rm -rf /data/data/com.google.android.gms/app_dg_cache/ >/dev/null 2>&1

sleep 2
pkill -f "com.google.android.gms.unstable" >/dev/null 2>&1 || true

exit 0
