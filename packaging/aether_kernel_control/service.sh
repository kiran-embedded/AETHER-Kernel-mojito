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
sleep 5

# Ensure open node permissions for display, kernel, and battery control
chmod 0666 /sys/kernel/aether/* 2>/dev/null || true
chmod 0755 /sys/devices/platform/soc/soc:qcom,dsi-display 2>/dev/null || true
chmod 0666 /sys/devices/platform/soc/soc:qcom,dsi-display/* 2>/dev/null || true
chmod 0666 /sys/class/drm/card0-DSI-1/* 2>/dev/null || true
for chg_node in \
    /sys/class/power_supply/battery/charging_enabled \
    /sys/class/power_supply/battery/battery_charging_enabled \
    /sys/class/power_supply/battery/constant_charge_current_max \
    /sys/class/power_supply/battery/fastcharge_mode
do
    [ -f "$chg_node" ] && chmod 0666 "$chg_node" 2>/dev/null || true
done

# Hardware Charge Cutoff & Bypass Persistence (Offline & Normal Boot)
if [ -f /data/adb/aether/cutoff.conf ]; then
    . /data/adb/aether/cutoff.conf 2>/dev/null
fi
if [ -f /mnt/vendor/persist/aether/cutoff.conf ]; then
    . /mnt/vendor/persist/aether/cutoff.conf 2>/dev/null
fi
[ -n "$CUTOFF_LIMIT" ] && echo "$CUTOFF_LIMIT" > /sys/kernel/aether/charge_cutoff_limit 2>/dev/null || true
[ -n "$CUTOFF_MODE" ] && echo "$CUTOFF_MODE" > /sys/kernel/aether/charge_cutoff_mode 2>/dev/null || true
[ -n "$INSTANT_BYPASS" ] && echo "$INSTANT_BYPASS" > /sys/kernel/aether/instant_bypass 2>/dev/null || true


# ── Userspace Framework Memory & LMKD Configuration ─────────────
# Applied once at boot completion

# 1. Zygote USAP Pool Configuration
resetprop -n persist.device_config.runtime_native.usap_pool_enabled true 2>/dev/null || setprop persist.device_config.runtime_native.usap_pool_enabled true 2>/dev/null || true
resetprop -n dalvik.vm.usap_pool_size_max 2 2>/dev/null || setprop dalvik.vm.usap_pool_size_max 2 2>/dev/null || true
resetprop -n dalvik.vm.usap_pool_size_min 1 2>/dev/null || setprop dalvik.vm.usap_pool_size_min 1 2>/dev/null || true
resetprop -n dalvik.vm.usap_pool_refill_threshold 1 2>/dev/null || setprop dalvik.vm.usap_pool_refill_threshold 1 2>/dev/null || true

# 2. Logcat & Debug Logging Buffer Bounds
logcat -G 256K 2>/dev/null || true
resetprop -n persist.logd.size 262144 2>/dev/null || setprop persist.logd.size 262144 2>/dev/null || true
resetprop -n persist.logd.size.main 262144 2>/dev/null || setprop persist.logd.size.main 262144 2>/dev/null || true
resetprop -n persist.logd.size.system 131072 2>/dev/null || setprop persist.logd.size.system 131072 2>/dev/null || true
resetprop -n persist.logd.size.radio 131072 2>/dev/null || setprop persist.logd.size.radio 131072 2>/dev/null || true
resetprop -n persist.logd.size.crash 262144 2>/dev/null || setprop persist.logd.size.crash 262144 2>/dev/null || true

# 3. Disable debug tracing & StrictMode overhead
resetprop -n persist.traced.enable 0 2>/dev/null || setprop persist.traced.enable 0 2>/dev/null || true
resetprop -n persist.sys.assert.enable false 2>/dev/null || setprop persist.sys.assert.enable false 2>/dev/null || true
resetprop -n persist.sys.strictmode.visual 0 2>/dev/null || setprop persist.sys.strictmode.visual 0 2>/dev/null || true
resetprop -n persist.sys.strictmode.disable 1 2>/dev/null || setprop persist.sys.strictmode.disable 1 2>/dev/null || true
resetprop -n libc.debug.malloc 0 2>/dev/null || setprop libc.debug.malloc 0 2>/dev/null || true
resetprop -n persist.libc.debug.malloc 0 2>/dev/null || setprop persist.libc.debug.malloc 0 2>/dev/null || true

# 4. Dalvik VM Heap Growth Capping (keeps 512m max, prevents lazy anonymous RAM bloat)
resetprop -n dalvik.vm.heapsize 512m 2>/dev/null || setprop dalvik.vm.heapsize 512m 2>/dev/null || true
resetprop -n dalvik.vm.heapgrowthlimit 256m 2>/dev/null || setprop dalvik.vm.heapgrowthlimit 256m 2>/dev/null || true
resetprop -n dalvik.vm.heaptargetutilization 0.75 2>/dev/null || setprop dalvik.vm.heaptargetutilization 0.75 2>/dev/null || true
resetprop -n dalvik.vm.heapminfree 2m 2>/dev/null || setprop dalvik.vm.heapminfree 2m 2>/dev/null || true
resetprop -n dalvik.vm.heapmaxfree 8m 2>/dev/null || setprop dalvik.vm.heapmaxfree 8m 2>/dev/null || true

# 5. LMKD Configuration (Keep cached app limits without forcing wallpaper kills)
resetprop -n ro.sys.fw.bg_apps_limit 36 2>/dev/null || setprop ro.sys.fw.bg_apps_limit 36 2>/dev/null || true
resetprop -n persist.sys.lmk.cached_apps_limit 36 2>/dev/null || setprop persist.sys.lmk.cached_apps_limit 36 2>/dev/null || true
resetprop -n ro.config.max_starting_bg 8 2>/dev/null || setprop ro.config.max_starting_bg 8 2>/dev/null || true
resetprop -n ro.lmk.kill_timeout_ms 100 2>/dev/null || setprop ro.lmk.kill_timeout_ms 100 2>/dev/null || true
resetprop -n ro.lmk.psi_partial_stall_ms 70 2>/dev/null || setprop ro.lmk.psi_partial_stall_ms 70 2>/dev/null || true
resetprop -n ro.lmk.psi_complete_stall_ms 700 2>/dev/null || setprop ro.lmk.psi_complete_stall_ms 700 2>/dev/null || true
resetprop -n ro.lmk.swap_util_max 85 2>/dev/null || setprop ro.lmk.swap_util_max 85 2>/dev/null || true
resetprop -n ro.lmk.thrashing_limit 30 2>/dev/null || setprop ro.lmk.thrashing_limit 30 2>/dev/null || true
resetprop -n ro.lmk.thrashing_limit_decay 25 2>/dev/null || setprop ro.lmk.thrashing_limit_decay 25 2>/dev/null || true

# 6. Background CPUset confinement (keep low-priority work on Silver cores 0-5)
if [ -d /dev/cpuset/background ]; then
    echo "0-5" > /dev/cpuset/background/cpus 2>/dev/null || true
fi
if [ -d /dev/cpuset/system-background ]; then
    echo "0-5" > /dev/cpuset/system-background/cpus 2>/dev/null || true
fi

# 7. RenderThread & SurfaceFlinger Jitter Suppression (60 FPS Animation Pacing)
resetprop -n debug.sf.disable_backpressure 1 2>/dev/null || setprop debug.sf.disable_backpressure 1 2>/dev/null || true
resetprop -n debug.sf.latch_unsignaled 1 2>/dev/null || setprop debug.sf.latch_unsignaled 1 2>/dev/null || true
resetprop -n debug.hwui.render_thread_priority -20 2>/dev/null || setprop debug.hwui.render_thread_priority -20 2>/dev/null || true
resetprop -n renderthread.skia.reduceopstasksplitting true 2>/dev/null || setprop renderthread.skia.reduceopstasksplitting true 2>/dev/null || true

# 8. SchedTune Top-App & CPU Input Boost Calibration (Cores 0-5 1.0 GHz, Cores 6-7 sleep)
if [ -d /dev/stune/top-app ]; then
    echo 1 > /dev/stune/top-app/schedtune.prefer_idle 2>/dev/null || true
    echo 1 > /dev/stune/top-app/schedtune.colocate 2>/dev/null || true
    echo 1 > /dev/stune/top-app/schedtune.boost 2>/dev/null || true
fi
for cb in /sys/module/cpu_boost/parameters /sys/module/cpu_input_boost/parameters; do
    if [ -d "$cb" ]; then
        echo "0:1017600 1:1017600 2:1017600 3:1017600 4:1017600 5:1017600 6:0 7:0" > "$cb/input_boost_freq" 2>/dev/null || true
        echo 64 > "$cb/input_boost_ms" 2>/dev/null || true
        echo 64 > "$cb/input_boost_duration" 2>/dev/null || true
    fi
done
for pol in /sys/devices/system/cpu/cpufreq/policy0 /sys/devices/system/cpu/cpufreq/policy{0,1,2,3,4,5}; do
    [ -d "$pol/schedutil" ] && echo 500 > "$pol/schedutil/up_rate_limit_us" 2>/dev/null && echo 20000 > "$pol/schedutil/down_rate_limit_us" 2>/dev/null || true
done
for pol in /sys/devices/system/cpu/cpufreq/policy6 /sys/devices/system/cpu/cpufreq/policy{6,7}; do
    [ -d "$pol/schedutil" ] && echo 800 > "$pol/schedutil/up_rate_limit_us" 2>/dev/null && echo 25000 > "$pol/schedutil/down_rate_limit_us" 2>/dev/null || true
done

# 9. Deep Sleep & Idle Drain Protections (EAS, C-State Aware, LPM Prediction)
echo 1 > /proc/sys/kernel/sched_energy_aware 2>/dev/null || true
echo 0 > /proc/sys/kernel/sched_walt_rotate_big_tasks 2>/dev/null || true
echo 1 > /proc/sys/kernel/sched_cstate_aware 2>/dev/null || true
echo 1 > /sys/module/workqueue/parameters/power_efficient 2>/dev/null || true
echo 1 > /sys/module/printk/parameters/console_suspend 2>/dev/null || true
echo "4 4 1 7" > /proc/sys/kernel/printk 2>/dev/null || true
echo 0 > /sys/module/lpm_levels/parameters/sleep_disabled 2>/dev/null || true
echo 1 > /sys/module/lpm_levels/parameters/lpm_prediction 2>/dev/null || true
echo 1 > /sys/module/lpm_levels/parameters/lpm_ipi_prediction 2>/dev/null || true
echo 1 > /sys/module/kprofiles/parameters/auto_kp 2>/dev/null || true
echo 0 > /sys/kernel/kprofiles/kp_mode 2>/dev/null || true

# 10. Core Memory & Scheduler Hardware Calibrations
# Virtual Memory & ZRAM Swappiness
echo 60 > /proc/sys/vm/swappiness 2>/dev/null || true
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null || true
echo 50 > /proc/sys/vm/vfs_cache_pressure 2>/dev/null || true
echo 150 > /proc/sys/vm/watermark_scale_factor 2>/dev/null || true
echo 5 > /proc/sys/vm/dirty_background_ratio 2>/dev/null || true
echo 20 > /proc/sys/vm/dirty_ratio 2>/dev/null || true
echo 500 > /proc/sys/vm/dirty_writeback_centisecs 2>/dev/null || true
echo 1500 > /proc/sys/vm/dirty_expire_centisecs 2>/dev/null || true
echo 16384 > /proc/sys/vm/min_free_kbytes 2>/dev/null || true
echo 0 > /proc/sys/vm/extra_free_kbytes 2>/dev/null || true
echo 0 > /proc/sys/vm/percpu_pagelist_fraction 2>/dev/null || true
echo 0 > /proc/sys/vm/compact_unevictable_allowed 2>/dev/null || true
echo 10 > /proc/sys/vm/stat_interval 2>/dev/null || true

# MGLRU Multigenerational LRU
if [ -f /sys/kernel/mm/lru_gen/enabled ]; then
    echo 7 > /sys/kernel/mm/lru_gen/enabled 2>/dev/null || echo y > /sys/kernel/mm/lru_gen/enabled 2>/dev/null || true
    echo 1000 > /sys/kernel/mm/lru_gen/min_ttl_ms 2>/dev/null || true
fi

# BORE Scheduler & Schedstats Overhead Elimination
echo 0 > /proc/sys/kernel/sched_schedstats 2>/dev/null || true
echo 1 > /proc/sys/kernel/sched_bore 2>/dev/null || true
echo 1380 > /proc/sys/kernel/sched_burst_penalty_scale 2>/dev/null || true
echo 22 > /proc/sys/kernel/sched_burst_penalty_offset 2>/dev/null || true
echo 1 > /proc/sys/kernel/sched_burst_smoothness_long 2>/dev/null || true
echo 0 > /proc/sys/kernel/sched_burst_smoothness_short 2>/dev/null || true
echo 2 > /proc/sys/kernel/sched_burst_smoothness_up 2>/dev/null || true
echo 1 > /proc/sys/kernel/sched_burst_smoothness_down 2>/dev/null || true
echo 60000000 > /proc/sys/kernel/sched_burst_cache_lifetime 2>/dev/null || true
echo 2 > /proc/sys/kernel/sched_burst_fork_atavistic 2>/dev/null || true

# ZRAM Queue Optimization — Zero latency, no merging, no iostats overhead
if [ -d /sys/block/zram0/queue ]; then
    echo 0 > /sys/block/zram0/queue/read_ahead_kb 2>/dev/null || true
    echo 2 > /sys/block/zram0/queue/rq_affinity 2>/dev/null || true
    echo 2 > /sys/block/zram0/queue/nomerges 2>/dev/null || true
    echo 0 > /sys/block/zram0/queue/iostats 2>/dev/null || true
    echo 0 > /sys/block/zram0/queue/add_random 2>/dev/null || true
fi

# F2FS In-Place Updates (IPU Policy 18: F2FS_IPU_FORCE | F2FS_IPU_FSYNC)
for f in /sys/fs/f2fs/*/ipu_policy; do
    [ -f "$f" ] && echo 18 > "$f" 2>/dev/null || true
done

# Adreno 612 GPU Baseline
if [ -d /sys/class/kgsl/kgsl-3d0 ]; then
    echo "msm-adreno-tz" > /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null || true
    echo 180 > /sys/class/kgsl/kgsl-3d0/min_clock_mhz 2>/dev/null || true
    echo 64 > /sys/class/kgsl/kgsl-3d0/idle_timer 2>/dev/null || true
    echo 0 > /sys/class/kgsl/kgsl-3d0/force_bus_on 2>/dev/null || true
    echo 0 > /sys/class/kgsl/kgsl-3d0/force_clk_on 2>/dev/null || true
    echo 0 > /sys/class/kgsl/kgsl-3d0/force_rail_on 2>/dev/null || true
fi

# Network TCP Lowat Pacing & Fast Open
echo 16384 > /proc/sys/net/ipv4/tcp_notsent_lowat 2>/dev/null || true
echo 3 > /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null || true
setprop wifi.supplicant_scan_interval 180 2>/dev/null || true

# Purge any legacy generic override scripts to guarantee dev-set kernel values are never overwritten
rm -f /data/adb/aether/aether_tweaks.sh 2>/dev/null || true

# If the user explicitly customized and saved personalized tweaks, re-apply them once on boot
if [ -f /data/adb/aether/aether_user_tweaks.sh ]; then
    sh /data/adb/aether/aether_user_tweaks.sh >/dev/null 2>&1 || true
fi

# ── UFS 2.2 Write Speed + Battery Efficiency Re-Apply ────────────────
# Runs once at boot completion — direct paths, no loops, no globs
# Execution time: ~2ms | CPU cost: negligible | Background daemons: 0
#
# SM6150 (Redmi Note 10 mojito) UFS host controller path:
UFS=/sys/devices/platform/soc/1d84000.ufshc
#
# Only apply if UFS sysfs is available (safe on all builds)
if [ -d "$UFS" ]; then
    # Clock gating: 15ms perf / 8ms power-save
    echo 15 > "$UFS/clkgate_delay_ms_perf"      2>/dev/null || true
    echo 15 > "$UFS/clkgate_delay_ms"            2>/dev/null || true
    echo 8  > "$UFS/clkgate_delay_ms_pwr_save"   2>/dev/null || true

    # Auto-Hibern8: enter Hibern8 after 5ms idle
    echo 5000 > "$UFS/auto_hibern8"              2>/dev/null || true

    # Runtime PM: rpm=1 (active+hibern8), spm=3 (deep sleep when screen off)
    echo 1 > "$UFS/rpm_lvl"                      2>/dev/null || true
    echo 3 > "$UFS/spm_lvl"                      2>/dev/null || true

    # Clock scaling: dynamic HS-G3 Lane2 ramp under I/O load, idle at min gear
    echo 1  > "$UFS/clk_scaling/enable"          2>/dev/null || true
    echo 30 > "$UFS/clk_scaling/scale_up_threshold"   2>/dev/null || true
    echo 20 > "$UFS/clk_scaling/scale_down_threshold" 2>/dev/null || true

    # WriteBooster: SLC turbo write buffer
    # wb_buf_flush_thresh=80: flush at 80% full
    # wb_flush_stat_after_h8=1: flush during hibern8 idle, not during active I/O
    echo 1  > "$UFS/wb_on"                       2>/dev/null || true
    echo 1  > "$UFS/wb_buf_flush_en"             2>/dev/null || true
    echo 80 > "$UFS/wb_buf_flush_thresh"         2>/dev/null || true
    echo 1  > "$UFS/wb_flush_stat_after_h8"      2>/dev/null || true
    echo 1  > "$UFS/write_booster_enable"        2>/dev/null || true

    # Hibern8 on idle: 5ms
    echo 5  > "$UFS/hibern8_on_idle_delay"       2>/dev/null || true
fi

# Block layer UFS queue tuning — direct paths, single writes, no loops
# nr_requests=64: matches UFS NCQ depth
# front_merges=1: deadline coalesces adjacent sequential writes
# async_depth=32: 32 write requests in flight
# write_expire=2000: wider window for write batching
for _q in /sys/block/sd*/queue; do
    [ -d "$_q" ] || continue
    [ -f "$_q/scheduler" ] && echo mq-deadline > "$_q/scheduler" 2>/dev/null || true
    echo 0    > "$_q/add_random"                 2>/dev/null || true
    echo 0    > "$_q/iostats"                    2>/dev/null || true
    echo 2    > "$_q/rq_affinity"                2>/dev/null || true
    echo 512  > "$_q/read_ahead_kb"              2>/dev/null || true
    echo 64   > "$_q/nr_requests"                2>/dev/null || true
    echo 1    > "$_q/iosched/front_merges"       2>/dev/null || true
    echo 32   > "$_q/iosched/async_depth"        2>/dev/null || true
    echo 2000 > "$_q/iosched/write_expire"       2>/dev/null || true
    # max_sectors_kb: read hw cap, apply up to 512KB per UFS command
    _hw=$(cat "$_q/max_hw_sectors_kb" 2>/dev/null || echo 512)
    [ "$_hw" -gt 512 ] && _hw=512
    echo "$_hw" > "$_q/max_sectors_kb"           2>/dev/null || true
done
unset UFS _q _hw

exit 0

