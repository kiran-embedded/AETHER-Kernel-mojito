#!/system/bin/sh
# ========================================================
# AETHER Kernel Control - Clean Uninstaller & Full Wipe
# Reverts all hardware sysfs nodes to factory safe baseline,
# completely wipes all /data/adb/aether/ files, and deletes
# all hazardous persistent system properties to prevent bootloops.
# ZERO BLOCKING COMMANDS • ZERO TINYMIX CALLS • 100% SAFE
# ========================================================
MODDIR=${0%/*}

# 1. Battery & Power Subsystem (Restore stock in-kernel charging and clear bypass)
echo 0 > /sys/kernel/aether/instant_bypass 2>/dev/null || true
echo 0 > /sys/kernel/aether/charge_cutoff_limit 2>/dev/null || true
echo 0 > /sys/kernel/aether/charge_cutoff_mode 2>/dev/null || true
echo 0 > /sys/kernel/aether/charge_current_limit 2>/dev/null || true
echo 1 > /sys/class/power_supply/battery/fastcharge_mode 2>/dev/null || true
echo 1 > /sys/class/power_supply/bms/fastcharge_mode 2>/dev/null || true

# 2. AMOLED Display Subsystem (Restore Stock PWM)
echo 0 > /sys/kernel/aether/anti_flicker 2>/dev/null || true
echo 0 > /sys/kernel/aether/hbm 2>/dev/null || true
echo 0 > /sys/devices/virtual/graphics/fb0/anti_flicker 2>/dev/null || true
echo 0 > /sys/class/graphics/fb0/anti_flicker 2>/dev/null || true
[ -f /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker ] && echo 0 > /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker 2>/dev/null || true
[ -f /sys/devices/platform/soc/soc:qcom,dsi-display/hbm ] && echo 0 > /sys/devices/platform/soc/soc:qcom,dsi-display/hbm 2>/dev/null || true
echo 0 > /sys/devices/platform/soc/soc:qcom,dsi-display-primary/hbm 2>/dev/null || true
echo 0 > /sys/class/drm/card0/device/hbm 2>/dev/null || true

# 3. Touchscreen Digitizer (Restore Normal Sensitivity)
for g in /sys/bus/i2c/drivers/ft3418_ts/*/fts_glove_mode /sys/class/input/input*/fts_glove_mode /sys/devices/platform/soc/*.i2c/i2c-*/1-*/fts_glove_mode; do
    [ -f "$g" ] && echo 0 > "$g" 2>/dev/null || true
done

# 4. KProfiles (Restore Developer In-Kernel Auto State)
echo 1 > /sys/module/kprofiles/parameters/auto_kp 2>/dev/null || true
echo 0 > /sys/kernel/kprofiles/kp_mode 2>/dev/null || true

# 5. Adreno 612 GPU (Restore Qualcomm MSM Devfreq Governor)
echo "msm-adreno-tz" > /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null || true
[ -f /sys/class/kgsl/kgsl-3d0/gpu_governor ] && echo "msm-adreno-tz" > /sys/class/kgsl/kgsl-3d0/gpu_governor 2>/dev/null || true
echo 290 > /sys/class/kgsl/kgsl-3d0/min_clock_mhz 2>/dev/null || true
echo 290000000 > /sys/class/kgsl/kgsl-3d0/devfreq/min_freq 2>/dev/null || true
[ -f /sys/class/kgsl/kgsl-3d0/min_pwrlevel ] && echo 5 > /sys/class/kgsl/kgsl-3d0/min_pwrlevel 2>/dev/null || true
echo 64 > /sys/class/kgsl/kgsl-3d0/idle_timer 2>/dev/null || true

# 6. CPU Schedutil & Sultan Input Boost (Restore In-Kernel Baseline)
for pol in /sys/devices/system/cpu/cpufreq/policy*; do
    [ -d "$pol/schedutil" ] && echo 500 > "$pol/schedutil/up_rate_limit_us" 2>/dev/null || true
    [ -d "$pol/schedutil" ] && echo 20000 > "$pol/schedutil/down_rate_limit_us" 2>/dev/null || true
done
[ -f /sys/module/cpu_boost/parameters/input_boost_ms ] && echo 40 > /sys/module/cpu_boost/parameters/input_boost_ms 2>/dev/null || true
[ -f /sys/module/cpu_boost/parameters/input_boost_freq ] && echo "0:1017600 6:0" > /sys/module/cpu_boost/parameters/input_boost_freq 2>/dev/null || true
[ -f /sys/module/cpu_input_boost/parameters/input_boost_duration ] && echo 40 > /sys/module/cpu_input_boost/parameters/input_boost_duration 2>/dev/null || true

# 7. BORE CPU Scheduler (Restore Developer Burst Baseline)
echo 1 > /proc/sys/kernel/sched_bore 2>/dev/null || true
echo 1280 > /proc/sys/kernel/sched_burst_penalty_scale 2>/dev/null || true
echo 22 > /proc/sys/kernel/sched_burst_penalty_offset 2>/dev/null || true

# 8. Network & Memory Subsystems (Restore Developer Calibrations)
echo "bbr" > /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null || echo "cubic" > /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null || true
echo 1 > /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null || true
echo 16384 > /proc/sys/net/ipv4/tcp_notsent_lowat 2>/dev/null || true
echo 100 > /proc/sys/vm/swappiness 2>/dev/null || true
[ -f /proc/sys/vm/direct_vm_swappiness ] && echo 20 > /proc/sys/vm/direct_vm_swappiness 2>/dev/null || true
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null || true
echo 100 > /proc/sys/vm/vfs_cache_pressure 2>/dev/null || true
echo 200 > /proc/sys/vm/watermark_scale_factor 2>/dev/null || true
echo 10 > /proc/sys/vm/dirty_background_ratio 2>/dev/null || true
echo 20 > /proc/sys/vm/dirty_ratio 2>/dev/null || true
echo 500 > /proc/sys/vm/dirty_writeback_centisecs 2>/dev/null || true
echo 1500 > /proc/sys/vm/dirty_expire_centisecs 2>/dev/null || true
echo 16384 > /proc/sys/vm/min_free_kbytes 2>/dev/null || true
echo 0 > /proc/sys/vm/extra_free_kbytes 2>/dev/null || true
[ -f /sys/kernel/mm/lru_gen/min_ttl_ms ] && echo 1000 > /sys/kernel/mm/lru_gen/min_ttl_ms 2>/dev/null || true
for zq in /sys/block/zram0/queue; do
    if [ -d "$zq" ]; then
        echo 0 > "$zq/read_ahead_kb" 2>/dev/null || true
        echo 2 > "$zq/rq_affinity" 2>/dev/null || true
        echo 2 > "$zq/nomerges" 2>/dev/null || true
        echo 0 > "$zq/iostats" 2>/dev/null || true
        echo 0 > "$zq/add_random" 2>/dev/null || true
    fi
done

# 9. In-Kernel Audio Bypass (Stock Qualcomm Hardware Routing)
# Non-blocking in-kernel sysfs switches — ZERO tinymix userspace calls to prevent bootloop hangs
echo 1 > /sys/kernel/aether/audio_bypass_mode 2>/dev/null || true
echo 0 > /sys/kernel/aether/mic_hpf_mode 2>/dev/null || true
echo 5 > /sys/module/tx_macro/parameters/tx_unmute_delay 2>/dev/null || true
[ -f /sys/module/tx_macro_dlkm/parameters/tx_unmute_delay ] && echo 5 > /sys/module/tx_macro_dlkm/parameters/tx_unmute_delay 2>/dev/null || true

# 10. CRITICAL BOOTLOOP PROTECTION: Purge Poisonous Persistent Properties
# In Android Bionic, if persist.libc.debug.malloc is set to anything (even 0),
# bionic tries to initialize malloc debug, causing Zygote crashes and bootloop!
resetprop --delete persist.libc.debug.malloc 2>/dev/null || true
resetprop --delete libc.debug.malloc 2>/dev/null || true
resetprop --delete persist.sys.strictmode.disable 2>/dev/null || true
resetprop --delete persist.sys.strictmode.visual 2>/dev/null || true
resetprop --delete persist.sys.assert.enable 2>/dev/null || true
resetprop --delete debug.sf.disable_backpressure 2>/dev/null || true
resetprop --delete debug.sf.latch_unsignaled 2>/dev/null || true
resetprop --delete debug.hwui.render_thread_priority 2>/dev/null || true
resetprop --delete renderthread.skia.reduceopstasksplitting 2>/dev/null || true

# 11. Complete Daemon Stop & Non-blocking Cleanup (Zero Leftover Traces)
for p in $(pidof aetherd 2>/dev/null); do
    kill -9 "$p" 2>/dev/null || true
done

# Non-blocking APK removal: never hang early boot or recovery
if [ "$(getprop sys.boot_completed)" = "1" ] && which pm >/dev/null 2>&1; then
    (
        pm uninstall com.aether.engine.service >/dev/null 2>&1 || cmd package uninstall com.aether.engine.service >/dev/null 2>&1 || true
        pm uninstall com.aether.fpsmeter >/dev/null 2>&1 || cmd package uninstall com.aether.fpsmeter >/dev/null 2>&1 || true
    ) &
fi

# Clean temporary files & IPC endpoints
rm -f /data/local/tmp/*aether* /data/local/tmp/*Aether* /data/local/tmp/aether_fps 2>/dev/null || true
rm -rf /data/adb/aether
rm -f /data/adb/aether_boot_count /data/adb/aether_boot_status.log /data/adb/aether_safe_mode
rm -f /data/adb/aether_uninstall.log 2>/dev/null || true
rm -rf /mnt/vendor/persist/aether 2>/dev/null || true

# Thoroughly purge ALL service and post-fs-data scripts across Magisk, KernelSU, and APatch
rm -f /data/adb/service.d/*aether* /data/adb/service.d/*displayfeatures* 2>/dev/null || true
rm -f /data/adb/post-fs-data.d/*aether* /data/adb/post-fs-data.d/*displayfeatures* 2>/dev/null || true
rm -f /data/adb/ksu/service.d/*aether* /data/adb/ksu/service.d/*displayfeatures* 2>/dev/null || true
rm -f /data/adb/ksu/post-fs-data.d/*aether* /data/adb/ksu/post-fs-data.d/*displayfeatures* 2>/dev/null || true
rm -f /data/adb/ap/service.d/*aether* /data/adb/ap/service.d/*displayfeatures* 2>/dev/null || true
rm -f /data/adb/ap/post-fs-data.d/*aether* /data/adb/ap/post-fs-data.d/*displayfeatures* 2>/dev/null || true

exit 0
