#!/system/bin/sh
# ========================================================
# AETHER SuSFS Stock Spoofer - Post-FS-Data Hook
# Native Kernel-Level Hardware Identity & Stealth Engine
# ========================================================
MODDIR=${0%/*}

# Safety check: if module is marked for deletion, exit immediately
[ -f "$MODDIR/remove" ] && exit 0

# Locate ksu_susfs binary
SUSFS_BIN="/data/adb/ksu/bin/ksu_susfs"
if [ ! -x "$SUSFS_BIN" ]; then
    if [ -x "$MODDIR/tools/ksu_susfs_arm64" ]; then
        mkdir -p /data/adb/ksu/bin
        cp -f "$MODDIR/tools/ksu_susfs_arm64" "$SUSFS_BIN"
        chmod 755 "$SUSFS_BIN"
    else
        SUSFS_BIN=""
    fi
fi

# ========================================================
# Phase 1: Native SuSFS Stealth Isolation
# ========================================================
if [ -n "$SUSFS_BIN" ] && [ -x "$SUSFS_BIN" ]; then
    # 1. Hide root binaries, module folders, and traces from stat()/readdir()
    # ONLY hide root/su paths. NEVER hide /data/system/ or framework paths to prevent PMS crash.
    for p in \
        /data/adb/ksu \
        /data/adb/modules/aether_kernel_control \
        /data/adb/modules/aether_susfs_spoofer \
        /data/adb/aether \
        /data/adb/magisk \
        /data/adb/ap \
        /system/bin/su \
        /system/xbin/su \
        /sbin/su \
        /vendor/bin/su \
        /data/local/tmp/su
    do
        [ -e "$p" ] && "$SUSFS_BIN" add_sus_path "$p" >/dev/null 2>&1
    done
fi

exit 0
