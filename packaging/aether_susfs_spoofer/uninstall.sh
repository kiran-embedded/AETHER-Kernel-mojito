#!/system/bin/sh
# ========================================================
# AETHER SuSFS Stock Spoofer - Clean Uninstaller & Full Wipe
# ========================================================
MODDIR=${0%/*}

# 1. Clean temporary cmdline and bootconfig cache
rm -rf /data/adb/susfs_tmp
rm -rf /data/adb/aether_susfs

# 2. Remove any lingering init scripts
rm -f /data/adb/service.d/*susfs_spoofer*
rm -f /data/adb/post-fs-data.d/*susfs_spoofer*
rm -f /data/adb/modules_update/aether_susfs_spoofer

exit 0
