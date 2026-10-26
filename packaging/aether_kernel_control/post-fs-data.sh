#!/system/bin/sh
# ========================================================
# AETHER Kernel Control - Post-FS-Data Hook
# Ring-0 Node Permissions & Autonomous Bootloop Protection
# ========================================================
MODDIR=${0%/*}

# ── 1. Immediate Safety & Removal Guard ─────────────────
# If module is disabled or marked for removal, exit cleanly immediately
if [ -f "$MODDIR/disable" ] || [ -f "$MODDIR/remove" ]; then
    exit 0
fi

# Emergency manual safe mode triggers (e.g. from recovery terminal)
if [ -f /data/adb/aether_safe_mode ] || [ -f /cache/aether_safe_mode ] || [ -f /data/adb/aether/safe_mode ]; then
    touch "$MODDIR/disable"
    rm -f /data/adb/aether_safe_mode /cache/aether_safe_mode /data/adb/aether/safe_mode 2>/dev/null || true
    exit 0
fi

# ── 2. Hardware Volume-Down Key Recovery Intercept ──────
# If user holds Volume Down during early boot, immediately disable module.
# Provides a 100% reliable hardware rescue key without needing custom recovery.
if command -v timeout >/dev/null 2>&1 && command -v getevent >/dev/null 2>&1; then
    if timeout 1 getevent -l 2>/dev/null | grep -qi "KEY_VOLUMEDOWN"; then
        mkdir -p /data/adb/aether
        echo "[SAFE-MODE] Volume-Down held during boot! Auto-disabling module." > /data/adb/aether/boot_status.log
        touch "$MODDIR/disable"
        echo 0 > /data/adb/aether/boot_count 2>/dev/null
        echo 0 > /data/adb/aether_boot_count 2>/dev/null
        exit 0
    fi
fi

# ── 3. Autonomous 2-Boot Crash Watchdog (Bootloop Protection) ─
mkdir -p /data/adb/aether
chmod 755 /data/adb/aether

BOOT_COUNT=$(cat /data/adb/aether/boot_count 2>/dev/null || cat /data/adb/aether_boot_count 2>/dev/null || echo 0)
case "$BOOT_COUNT" in
    ''|*[!0-9]*) BOOT_COUNT=0 ;;
esac

if [ "$BOOT_COUNT" -ge 2 ]; then
    # Two consecutive boots failed to reach sys.boot_completed=1!
    # Autonomous bootloop protection triggers: disable module & wipe dangerous props to break bootloop.
    touch "$MODDIR/disable"
    echo 0 > /data/adb/aether/boot_count
    echo 0 > /data/adb/aether_boot_count 2>/dev/null
    echo "[AETHER-BOOTLOOP-GUARD] Detected 2 consecutive boot crashes! Auto-disabled module to safe-boot the ROM." > /data/adb/aether/boot_status.log

    # Purge any user tweaks script that might have caused a freeze
    rm -f /data/adb/aether/aether_user_tweaks.sh /data/adb/aether/aether_tweaks.sh 2>/dev/null || true

    # Actively purge dangerous persistent properties that break Bionic / Zygote
    resetprop --delete persist.libc.debug.malloc 2>/dev/null || true
    resetprop --delete libc.debug.malloc 2>/dev/null || true
    resetprop --delete persist.sys.strictmode.disable 2>/dev/null || true
    resetprop --delete persist.sys.strictmode.visual 2>/dev/null || true
    resetprop --delete persist.sys.assert.enable 2>/dev/null || true
    resetprop --delete debug.sf.disable_backpressure 2>/dev/null || true
    resetprop --delete debug.sf.latch_unsignaled 2>/dev/null || true
    resetprop --delete debug.hwui.render_thread_priority 2>/dev/null || true
    resetprop --delete renderthread.skia.reduceopstasksplitting 2>/dev/null || true

    exit 0
fi

# Increment boot counter for this boot attempt (cleared by service.sh on sys.boot_completed=1)
BOOT_COUNT=$((BOOT_COUNT + 1))
echo "$BOOT_COUNT" > /data/adb/aether/boot_count
echo "$BOOT_COUNT" > /data/adb/aether_boot_count 2>/dev/null

# Clean up any poisonous malloc debug property immediately on every boot
resetprop --delete persist.libc.debug.malloc 2>/dev/null || true
resetprop --delete libc.debug.malloc 2>/dev/null || true

# ── 4. Ring-0 SysFS Node Permissions Initialization ─────
# 1. Allow system & webui to manipulate DC Dim, HBM, DRM disp_param and FPS Counter nodes
chown system:graphics /sys/class/drm/card*-DSI-*/disp_param /sys/devices/platform/soc/*.qcom,mdss_mdp/drm/card*/card*-DSI-*/disp_param 2>/dev/null || true
chmod 0666 /sys/class/drm/card*-DSI-*/disp_param /sys/devices/platform/soc/*.qcom,mdss_mdp/drm/card*/card*-DSI-*/disp_param 2>/dev/null || true
chown system:system /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/anti_flicker 2>/dev/null || true
chmod 0666 /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/anti_flicker 2>/dev/null || true

chown system:system /sys/devices/platform/soc/soc:qcom,dsi-display/hbm /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/hbm 2>/dev/null || true
chmod 0666 /sys/devices/platform/soc/soc:qcom,dsi-display/hbm /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/hbm 2>/dev/null || true

chown system:graphics /sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/sde-crtc-0/measured_fps 2>/dev/null || true
chmod 0666 /sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/sde-crtc-0/measured_fps 2>/dev/null || true

# 2. Allow system & webui to manipulate KProfiles modes and parameters
chown system:system /sys/module/kprofiles/parameters/auto_kp 2>/dev/null || true
chmod 0666 /sys/module/kprofiles/parameters/auto_kp 2>/dev/null || true

chown system:system /sys/kernel/kprofiles/kp_mode 2>/dev/null || true
chmod 0666 /sys/kernel/kprofiles/kp_mode 2>/dev/null || true

# 3. Allow system & webui to manipulate Aether in-kernel control nodes
chown system:system /sys/kernel/aether/* 2>/dev/null || true
chmod 0666 /sys/kernel/aether/* 2>/dev/null || true

# 4. Safe live Magisk / KernelSU policy injection
#    ANDROID 17 FIX: Expanded for shell/untrusted_app sysfs access
#    required by KernelSU-Next WebUI command execution on A17.
if command -v magiskpolicy >/dev/null 2>&1; then
    magiskpolicy --live "allow init sysfs file setattr" 2>/dev/null || true
    magiskpolicy --live "allow system_app vendor_sysfs_graphics dir search" 2>/dev/null || true
    magiskpolicy --live "allow system_app vendor_sysfs_graphics file { ioctl read write getattr lock append map open watch watch_reads }" 2>/dev/null || true
    magiskpolicy --live "allow system_app sysfs dir search" 2>/dev/null || true
    magiskpolicy --live "allow system_app sysfs file { ioctl read write getattr open }" 2>/dev/null || true
    magiskpolicy --live "allow shell sysfs dir { search read open getattr }" 2>/dev/null || true
    magiskpolicy --live "allow shell sysfs file { ioctl read write getattr open }" 2>/dev/null || true
    magiskpolicy --live "allow shell sysfs_type dir { search read open getattr }" 2>/dev/null || true
    magiskpolicy --live "allow shell sysfs_type file { ioctl read write getattr open }" 2>/dev/null || true
    magiskpolicy --live "allow untrusted_app sysfs dir { search read open getattr }" 2>/dev/null || true
    magiskpolicy --live "allow untrusted_app sysfs file { ioctl read getattr open }" 2>/dev/null || true
    magiskpolicy --live "allow platform_app sysfs dir { search read open getattr }" 2>/dev/null || true
    magiskpolicy --live "allow platform_app sysfs file { ioctl read write getattr open }" 2>/dev/null || true
fi

exit 0
