SKIPUNZIP=0

ui_print " "
ui_print "  ╔════════════════════════════════════════════════╗"
ui_print "  ║   A E T H E R   S U S F S   S P O O F E R      ║"
ui_print "  ║    Stock Redmi Note 10 Identity & GPay Fix     ║"
ui_print "  ╠════════════════════════════════════════════════╣"
ui_print "  ║  Device   : Xiaomi Redmi Note 10 (mojito)      ║"
ui_print "  ║  Kernel   : Linux 4.14 AETHER v1.4             ║"
ui_print "  ║  Target   : Stock 4.14.190-perf+ & MIUI 14     ║"
ui_print "  ╚════════════════════════════════════════════════╝"
ui_print " "

ui_print "  [1/3] Deploying SuSFS Native Userspace Tools..."
DEST_BIN_DIR="/data/adb/ksu/bin"
mkdir -p "${DEST_BIN_DIR}"
if [ -f "${MODPATH}/tools/ksu_susfs_arm64" ]; then
    cp -f "${MODPATH}/tools/ksu_susfs_arm64" "${DEST_BIN_DIR}/ksu_susfs"
    chmod 755 "${DEST_BIN_DIR}/ksu_susfs"
    ui_print "        ├─ ksu_susfs binary installed to ${DEST_BIN_DIR}"
fi

ui_print "  [2/3] Configuring File Permissions & Hooks..."
set_perm_recursive "${MODPATH}" 0 0 0755 0644
set_perm "${MODPATH}/post-fs-data.sh" 0 0 0755
set_perm "${MODPATH}/service.sh" 0 0 0755
set_perm "${MODPATH}/action.sh" 0 0 0755
set_perm "${MODPATH}/uninstall.sh" 0 0 0755
if [ -f "${MODPATH}/tools/ksu_susfs_arm64" ]; then
    set_perm "${MODPATH}/tools/ksu_susfs_arm64" 0 0 0755
fi
ui_print "        ├─ Boot hooks & Clean Uninstaller armed"

ui_print "  [3/3] Setting Up Initial Cloaking & GPay Repair..."
ui_print "        ├─ Hardware Identity -> Xiaomi Redmi Note 10 (Global)"
ui_print "        ├─ Bootloader State  -> Locked / Green"
ui_print "        ├─ Mount Stealth     -> SuSFS Isolation Active for Apps"

# Deploy certified stock pif.json if missing
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
    ui_print "        ├─ Certified stock pif.json deployed (FIRST_API_LEVEL=25)"
fi

# Automatically wipe Google's hidden DroidGuard security failure cache
rm -f /data/data/com.google.android.gms/databases/dg.db* >/dev/null 2>&1
rm -rf /data/data/com.google.android.gms/app_dg_cache/ >/dev/null 2>&1
rm -rf /data/data/com.google.android.apps.walletnfcrel/cache/* >/dev/null 2>&1
ui_print "        └─ Stale DroidGuard attestation cache wiped cleanly"

ui_print " "
ui_print "  ════════════════════════════════════════════════"
ui_print "  ✨ FLASH COMPLETE! REBOOT TO APPLY."
ui_print "  Google Pay & Banking Apps will detect device"
ui_print "  as official locked stock Xiaomi Redmi Note 10."
ui_print "  TIP: Click 'Action' in KernelSU anytime to"
ui_print "  auto-repair Play Integrity and wipe GMS cache!"
ui_print "  ════════════════════════════════════════════════"
ui_print " "
