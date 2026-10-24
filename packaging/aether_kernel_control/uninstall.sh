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
echo 1 > /sys/kernel/aether/charge_cutoff_mode 2>/dev/null

# 2. AMOLED Display Subsystem (Restore Stock PWM)
echo 0 > /sys/kernel/aether/anti_flicker 2>/dev/null
echo 0 > /sys/kernel/aether/hbm 2>/dev/null
echo 0 > /sys/devices/virtual/graphics/fb0/anti_flicker 2>/dev/null
echo 0 > /sys/class/graphics/fb0/anti_flicker 2>/dev/null
[ -f /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker ] && echo 0 > /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker 2>/dev/null
[ -f /sys/devices/platform/soc/soc:qcom,dsi-display/hbm ] && echo 0 > /sys/devices/platform/soc/soc:qcom,dsi-display/hbm 2>/dev/null
echo 0 > /sys/devices/platform/soc/soc:qcom,dsi-display-primary/hbm 2>/dev/null
echo 0 > /sys/class/drm/card0/device/hbm 2>/dev/null

# 3. Touchscreen Digitizer (Restore Normal Sensitivity)
for g in /sys/bus/i2c/drivers/ft3418_ts/*/fts_glove_mode /sys/class/input/input*/fts_glove_mode /sys/devices/platform/soc/*.i2c/i2c-*/1-*/fts_glove_mode; do
    [ -f "$g" ] && echo 0 > "$g" 2>/dev/null
done

# 4. KProfiles (Restore Developer In-Kernel Auto State)
echo 1 > /sys/module/kprofiles/parameters/auto_kp 2>/dev/null
echo 0 > /sys/kernel/kprofiles/kp_mode 2>/dev/null

# 5. Adreno 612 GPU (Restore Qualcomm MSM Devfreq Governor)
echo "msm-adreno-tz" > /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null
echo "msm-adreno-tz" > /sys/class/kgsl/kgsl-3d0/gpu_governor 2>/dev/null
echo 180 > /sys/class/kgsl/kgsl-3d0/min_clock_mhz 2>/dev/null
echo 66 > /sys/class/kgsl/kgsl-3d0/idle_timer 2>/dev/null

# 6. CPU Schedutil & Sultan Input Boost (Restore Developer In-Kernel Defaults)
for pol in /sys/devices/system/cpu/cpufreq/policy*; do
    [ -d "$pol/schedutil" ] && echo 500 > "$pol/schedutil/up_rate_limit_us" 2>/dev/null
    [ -d "$pol/schedutil" ] && echo 20000 > "$pol/schedutil/down_rate_limit_us" 2>/dev/null
done
[ -f /sys/module/cpu_boost/parameters/input_boost_ms ] && echo 64 > /sys/module/cpu_boost/parameters/input_boost_ms 2>/dev/null
[ -f /sys/module/cpu_boost/parameters/input_boost_freq ] && echo "0:979200 6:1305600" > /sys/module/cpu_boost/parameters/input_boost_freq 2>/dev/null
[ -f /sys/module/cpu_input_boost/parameters/input_boost_duration ] && echo 64 > /sys/module/cpu_input_boost/parameters/input_boost_duration 2>/dev/null
[ -f /sys/module/cpu_input_boost/parameters/input_boost_freq_lp ] && echo 979200 > /sys/module/cpu_input_boost/parameters/input_boost_freq_lp 2>/dev/null
[ -f /sys/module/cpu_input_boost/parameters/input_boost_freq_hp ] && echo 1305600 > /sys/module/cpu_input_boost/parameters/input_boost_freq_hp 2>/dev/null

# 7. BORE CPU Scheduler (Restore Developer Burst Penalty)
echo 1 > /proc/sys/kernel/sched_bore 2>/dev/null
echo 1380 > /proc/sys/kernel/sched_burst_penalty_scale 2>/dev/null
echo 22 > /proc/sys/kernel/sched_burst_penalty_offset 2>/dev/null
echo 2 > /proc/sys/kernel/sched_burst_smoothness_up 2>/dev/null
echo 1 > /proc/sys/kernel/sched_burst_smoothness_down 2>/dev/null

# 8. Network & Memory Subsystems (Restore Developer Calibrations)
echo "bbr" > /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null || echo "cubic" > /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null
echo 1 > /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null
echo 16384 > /proc/sys/net/ipv4/tcp_notsent_lowat 2>/dev/null
echo 60 > /proc/sys/vm/swappiness 2>/dev/null
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null
echo 100 > /proc/sys/vm/vfs_cache_pressure 2>/dev/null
echo 150 > /proc/sys/vm/watermark_scale_factor 2>/dev/null
[ -f /sys/kernel/mm/lru_gen/min_ttl_ms ] && echo 1000 > /sys/kernel/mm/lru_gen/min_ttl_ms 2>/dev/null
echo 512 > /sys/block/sda/queue/read_ahead_kb 2>/dev/null

# 9. Audio Codec & Mic Gains (Restore Stock Levels & In-Kernel Audio Bypass)
echo 1 > /sys/kernel/aether/audio_bypass_mode 2>/dev/null
echo 0 > /sys/kernel/aether/mic_hpf_mode 2>/dev/null
tinymix 'RX_HPH_PWR_MODE' 'NORMAL' 2>/dev/null || true
tinymix 'TX_DEC0 Volume' 84 2>/dev/null || true
tinymix 'TX_DEC1 Volume' 84 2>/dev/null || true
tinymix 'RX_EAR Volume' 84 2>/dev/null || true
echo 5 > /sys/module/tx_macro/parameters/tx_unmute_delay 2>/dev/null
[ -f /sys/module/tx_macro_dlkm/parameters/tx_unmute_delay ] && echo 5 > /sys/module/tx_macro_dlkm/parameters/tx_unmute_delay 2>/dev/null

# 10. Complete File Wipe (Zero Leftover Traces)
rm -rf /data/adb/aether
rm -f /data/adb/service.d/*aether*
rm -f /data/adb/post-fs-data.d/*aether*
rm -f /data/adb/aether_uninstall.log
rm -rf /mnt/vendor/persist/aether 2>/dev/null

exit 0
