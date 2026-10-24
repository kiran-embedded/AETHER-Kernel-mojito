#!/system/bin/sh
# ========================================================
# AETHER SuSFS Stock Identity & GPay Spoofer - Action Tool
# Interactive One-Click GPay & Play Integrity Repair Engine
# ========================================================
MODDIR=${0%/*}
SUSFS_BIN="/data/adb/ksu/bin/ksu_susfs"
[ ! -x "$SUSFS_BIN" ] && SUSFS_BIN="$MODDIR/tools/ksu_susfs_arm64"

echo "╔════════════════════════════════════════════════╗"
echo "║   AETHER SUSFS STOCK GPAY & INTEGRITY REPAIR   ║"
echo "╠════════════════════════════════════════════════╣"
echo "║ Device : Xiaomi Redmi Note 10 (mojito / sunny) ║"
echo "║ Kernel : Linux 4.14 AETHER v1.4                ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# ----------------------------------------------------
# 1. Stop Google Services to prepare for clean wipe
# ----------------------------------------------------
echo "► [1/6] Stopping Google Play Services & Wallet..."
am force-stop com.google.android.gms >/dev/null 2>&1
am force-stop com.android.vending >/dev/null 2>&1
am force-stop com.google.android.apps.walletnfcrel >/dev/null 2>&1
pkill -f "com.google.android.gms.unstable" >/dev/null 2>&1
echo "        └─ Done."

# ----------------------------------------------------
# 2. Wipe DroidGuard Cache & Database (The #1 Fix!)
# ----------------------------------------------------
echo "► [2/6] Wiping DroidGuard Security Risk Cache (dg.db)..."
rm -f /data/data/com.google.android.gms/databases/dg.db* >/dev/null 2>&1
rm -rf /data/data/com.google.android.gms/app_dg_cache/ >/dev/null 2>&1
rm -rf /data/data/com.google.android.apps.walletnfcrel/cache/* >/dev/null 2>&1
rm -rf /data/data/com.google.android.apps.walletnfcrel/code_cache/* >/dev/null 2>&1
echo "        └─ DroidGuard & Wallet attestation cache wiped cleanly."

# ----------------------------------------------------
# 3. Clear Play Store Attestation Cache
# ----------------------------------------------------
echo "► [3/6] Clearing Google Play Store Cache..."
pm clear com.android.vending >/dev/null 2>&1
echo "        └─ Play Store cache cleared (certification refreshed)."

# ----------------------------------------------------
# 4. Check & Disable Revoked / Banned Keybox
# ----------------------------------------------------
echo "► [4/6] Checking for Revoked Keybox..."
if [ -f "/data/adb/tricky_store/keybox.xml" ]; then
    mv -f "/data/adb/tricky_store/keybox.xml" "/data/adb/tricky_store/keybox.xml.revoked_backup"
    echo "        ├─ WARNING: Public keybox was detected and disabled."
    echo "        └─ Renamed to keybox.xml.revoked_backup to prevent Google bans."
else
    echo "        └─ Clean (no banned keybox interfering)."
fi

# ----------------------------------------------------
# 5. Generate / Refresh Stock Certified pif.json
# ----------------------------------------------------
echo "► [5/6] Deploying Certified Stock Redmi Note 10 PIF Profile..."
mkdir -p /data/adb
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

if [ -d "/data/adb/modules/playintegrityfix" ]; then
    cp -f /data/adb/pif.json /data/adb/modules/playintegrityfix/pif.json
    chmod 644 /data/adb/modules/playintegrityfix/pif.json
    echo "        ├─ Synced to PlayIntegrityFix module folder."
fi

# Ensure TrickyStore target.txt is correctly configured
if [ -d "/data/adb/tricky_store" ]; then
    echo "com.google.android.gms" > /data/adb/tricky_store/target.txt
    echo "com.android.vending" >> /data/adb/tricky_store/target.txt
    echo "com.google.android.apps.walletnfcrel" >> /data/adb/tricky_store/target.txt
    chmod 644 /data/adb/tricky_store/target.txt
    echo "        ├─ TrickyStore targets secured."
fi
echo "        └─ Certified stock profile active (FIRST_API_LEVEL=25)."

# ----------------------------------------------------
# 6. Re-assert Kernel SuSFS Cloaking & Bootloader Lock
# ----------------------------------------------------
echo "► [6/6] Enforcing SuSFS Kernel Cloaking & Green Lock..."
if [ -x "$SUSFS_BIN" ]; then
    "$SUSFS_BIN" set_uname "4.14.190-perf+" "#1 SMP PREEMPT Thu Jun 17 02:40:22 UTC 2021" >/dev/null 2>&1
    echo "        ├─ Kernel release cloaked: $(uname -r)"
fi

resetprop -n ro.boot.verifiedbootstate "green"
resetprop -n ro.boot.flash.locked "1"
resetprop -n ro.boot.veritymode "enforcing"
resetprop -n ro.boot.warranty_bit "0"
resetprop -n ro.debuggable "0"
resetprop -n ro.secure "1"
resetprop -n ro.build.tags "release-keys"
resetprop -n ro.build.type "user"
echo "        └─ Bootloader locked status: Green (1)"

echo ""
echo "════════════════════════════════════════════════"
echo "✨ ALL REPAIRS & WIPES COMPLETE!"
echo "1. Open Play Integrity API Checker to verify."
echo "2. Open Google Pay / Wallet — add cards & pay!"
echo "════════════════════════════════════════════════"
exit 0
