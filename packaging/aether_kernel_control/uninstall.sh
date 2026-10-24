#!/system/bin/sh
# ========================================================
# AETHER Kernel Control - Clean Uninstaller & Full Wipe
# Reverts all hardware sysfs nodes to factory safe baseline
# and permanently removes all /data/adb/aether/ files.
# ========================================================
MODDIR=${0%/*}

# 1. Battery & Power Subsystem (Restore stock in-kernel charging and clear bypass)
echo 0 > /sys/kernel/aether/instant_bypass 2>/dev/null
echo 0 > /sys/kernel/aether/charge_cutoff_limit 2>/dev/null
rm -rf /mnt/vendor/persist/aether 2>/dev/null

# 2. AMOLED Display Subsystem (Restore Stock PWM)
echo 0 > /sys/devices/virtual/graphics/fb0/anti_flicker 2>/dev/null
echo 0 > /sys/class/graphics/fb0/anti_flicker 2>/dev/null
echo 0 > /sys/devices/platform/soc/soc:qcom,dsi-display-primary/hbm 2>/dev/null
echo 0 > /sys/class/drm/card0/device/hbm 2>/dev/null

# 3. Touchscreen Digitizer (Restore Normal Sensitivity)
for g in /sys/bus/i2c/drivers/ft3418_ts/*/fts_glove_mode /sys/class/input/input*/fts_glove_mode; do
    [ -f "$g" ] && echo 0 > "$g" 2>/dev/null
done

# 4. Haptic Linear Vibrator (Restore Safe Stock Xiaomi Baseline)
echo 0 > /sys/class/leds/vibrator/haptic_mode 2>/dev/null
echo 30 > /sys/class/leds/vibrator/click_duration 2>/dev/null
echo 2400 > /sys/class/leds/vibrator/vmax_mv 2>/dev/null

# 5. Adreno 612 GPU (Restore Qualcomm MSM Devfreq Governor)
echo "msm-adreno-tz" > /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null
echo "msm-adreno-tz" > /sys/class/kgsl/kgsl-3d0/gpu_governor 2>/dev/null
echo 180 > /sys/class/kgsl/kgsl-3d0/min_clock_mhz 2>/dev/null
echo 50 > /sys/class/kgsl/kgsl-3d0/idle_timer 2>/dev/null

# 6. CPU Schedutil & Sultan Input Boost (Restore Factory Defaults)
for pol in /sys/devices/system/cpu/cpufreq/policy*; do
    [ -d "$pol/schedutil" ] && echo 500 > "$pol/schedutil/up_rate_limit_us" 2>/dev/null
    [ -d "$pol/schedutil" ] && echo 1000 > "$pol/schedutil/down_rate_limit_us" 2>/dev/null
done
echo 64 > /sys/module/cpu_input_boost/parameters/input_boost_duration 2>/dev/null
echo 1113600 > /sys/module/cpu_input_boost/parameters/input_boost_freq_lp 2>/dev/null
echo 1209600 > /sys/module/cpu_input_boost/parameters/input_boost_freq_hp 2>/dev/null

# 7. BORE CPU Scheduler (Restore Default Penalty)
echo 1 > /proc/sys/kernel/sched_bore 2>/dev/null
echo 1 > /proc/sys/kernel/sched_burst_penalty_scale 2>/dev/null

# 8. Network & Memory Subsystems (Restore Cubic & Defaults)
echo "cubic" > /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null
echo 1 > /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null
echo 60 > /proc/sys/vm/swappiness 2>/dev/null
echo 100 > /proc/sys/vm/vfs_cache_pressure 2>/dev/null
echo 256 > /sys/block/sda/queue/read_ahead_kb 2>/dev/null

# 9. Audio Codec & Mic Gains (Restore Stock Levels)
tinymix 'RX_HPH_PWR_MODE' 'NORMAL' 2>/dev/null || true
tinymix 'TX_DEC0 Volume' 84 2>/dev/null || true
tinymix 'TX_DEC1 Volume' 84 2>/dev/null || true
tinymix 'RX_EAR Volume' 84 2>/dev/null || true
echo 5 > /sys/module/tx_macro/parameters/tx_unmute_delay 2>/dev/null

# 10. Complete File Wipe (Zero Leftover Traces)
rm -rf /data/adb/aether
rm -f /data/adb/service.d/*aether*
rm -f /data/adb/post-fs-data.d/*aether*
rm -f /data/adb/aether_uninstall.log

exit 0
