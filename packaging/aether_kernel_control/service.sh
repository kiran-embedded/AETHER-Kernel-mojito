#!/system/bin/sh
# ========================================================
# AETHER Kernel Control - Background Boot Service
# ========================================================
MODDIR=${0%/*}

# If module is marked for deletion, trigger clean uninstaller and exit immediately
if [ -f "$MODDIR/remove" ]; then
    [ -f "$MODDIR/uninstall.sh" ] && sh "$MODDIR/uninstall.sh" 2>/dev/null
    exit 0
fi

# Ensure state directory exists with secure permissions
mkdir -p /data/adb/aether
chmod 755 /data/adb/aether

# Safe Boot Completed Watchdog: Poll up to 60s max to prevent indefinite background hang
TIMEOUT=60
while [ "$TIMEOUT" -gt 0 ] && [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 2
    TIMEOUT=$((TIMEOUT - 2))
done

# If module was deleted during boot, do not apply tweaks
if [ -f "$MODDIR/remove" ]; then
    [ -f "$MODDIR/uninstall.sh" ] && sh "$MODDIR/uninstall.sh" 2>/dev/null
    exit 0
fi

# Wait for audioserver and vendor HALs to stabilize
sleep 6

# Run user saved hardware configuration if present
if [ -f /data/adb/aether/aether_tweaks.sh ]; then
    chmod 755 /data/adb/aether/aether_tweaks.sh
    sh /data/adb/aether/aether_tweaks.sh > /data/adb/aether/boot_apply_log.txt 2>&1
fi

# Multi-stage persistence passes to guarantee ALSA mixer and haptics after full audioserver XML loading
(
    sleep 10
    if [ -f /data/adb/aether/aether_tweaks.sh ]; then
        sh /data/adb/aether/aether_tweaks.sh >> /data/adb/aether/boot_apply_log.txt 2>&1
    fi
    sleep 15
    if [ -f /data/adb/aether/aether_tweaks.sh ]; then
        sh /data/adb/aether/aether_tweaks.sh >> /data/adb/aether/boot_apply_log.txt 2>&1
    fi
) &

exit 0
