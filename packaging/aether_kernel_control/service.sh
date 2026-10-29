#!/system/bin/sh
# ========================================================
# AETHER Kernel Control - Background Boot Service
# ========================================================
MODDIR=${0%/*}

# ── 1. Immediate Safety & Removal Guard ─────────────────
# If module is disabled or marked for removal, exit cleanly immediately
if [ -f "$MODDIR/disable" ] || [ -f "$MODDIR/remove" ]; then
    exit 0
fi

# Ensure state directory exists with secure permissions
mkdir -p /data/adb/aether/bin
chmod 755 /data/adb/aether /data/adb/aether/bin

# Ensure bundled ALSA tinymix engine is active (Dinodva Module Integration)
if [ -f "$MODDIR/system/bin/tinymix" ] && [ ! -f /data/adb/aether/bin/tinymix ]; then
    cp -f "$MODDIR/system/bin/tinymix" /data/adb/aether/bin/tinymix 2>/dev/null || true
    chmod 755 /data/adb/aether/bin/tinymix 2>/dev/null || true
fi

# Immediately purge poisonous malloc debug properties
resetprop --delete persist.libc.debug.malloc 2>/dev/null || true
resetprop --delete libc.debug.malloc 2>/dev/null || true

# ── 2. Safe Boot Completed Watchdog ─────────────────────
# Poll up to 60s max to prevent indefinite background hang
TIMEOUT=60
while [ "$TIMEOUT" -gt 0 ] && [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 2
    TIMEOUT=$((TIMEOUT - 2))
done

# If system never completed boot within 60s, DO NOT apply any tweaks!
# Exit cleanly to let the system stabilize without external interference.
if [ "$(getprop sys.boot_completed)" != "1" ]; then
    echo "[AETHER-BOOT-GUARD] sys.boot_completed timed out! Service tweaks aborted." > /data/adb/aether/boot_status.log
    exit 0
fi

# If module was deleted or disabled during boot, exit immediately
if [ -f "$MODDIR/disable" ] || [ -f "$MODDIR/remove" ]; then
    exit 0
fi

# ── 3. Reset Boot Failure Counter (Boot Confirmed Safe) ─
echo 0 > /data/adb/aether/boot_count 2>/dev/null
echo 0 > /data/adb/aether_boot_count 2>/dev/null
echo "[AETHER-BOOT-OK] Boot completed successfully. Watchdog counter reset to 0." > /data/adb/aether/boot_status.log

# Wait for audioserver and vendor HALs to stabilize
sleep 5

# Clean migration from user-installed app under /data/app to systemless /system/app
for pkg in com.aether.fpsmeter com.aether.engine.service; do
    pkg_path=$(pm path "$pkg" 2>/dev/null | head -n 1 | cut -d: -f2)
    case "$pkg_path" in
        /data/app/*)
            pm uninstall -k "$pkg" >/dev/null 2>&1 || true
            ;;
    esac
done

# Ensure Aether FPS Floating Meter APK is present
if [ -f "$MODDIR/system/app/AetherFpsMeter/AetherFpsMeter.apk" ] || [ -f "$MODDIR/tools/AetherFpsMeter.apk" ]; then
    _apk="$MODDIR/system/app/AetherFpsMeter/AetherFpsMeter.apk"
    [ ! -f "$_apk" ] && _apk="$MODDIR/tools/AetherFpsMeter.apk"

    if ! pm path com.aether.fpsmeter >/dev/null 2>&1; then
        pm install -r -d -g "$_apk" >/dev/null 2>&1 || true
    fi

    appops set --user 0 com.aether.fpsmeter SYSTEM_ALERT_WINDOW allow 2>/dev/null || \
    appops set com.aether.fpsmeter SYSTEM_ALERT_WINDOW allow 2>/dev/null || true

    pm grant --user 0 com.aether.fpsmeter android.permission.SYSTEM_ALERT_WINDOW 2>/dev/null || \
    pm grant com.aether.fpsmeter android.permission.SYSTEM_ALERT_WINDOW 2>/dev/null || true

    pm grant --user 0 com.aether.fpsmeter android.permission.POST_NOTIFICATIONS 2>/dev/null || \
    pm grant com.aether.fpsmeter android.permission.POST_NOTIFICATIONS 2>/dev/null || true

    # Background execution and idle optimization whitelist
    cmd appops set com.aether.fpsmeter RUN_IN_BACKGROUND allow 2>/dev/null || true
    cmd appops set com.aether.fpsmeter RUN_ANY_IN_BACKGROUND allow 2>/dev/null || true
    cmd appops set com.aether.engine.service RUN_IN_BACKGROUND allow 2>/dev/null || true
    cmd appops set com.aether.engine.service RUN_ANY_IN_BACKGROUND allow 2>/dev/null || true

    cmd deviceidle whitelist +com.aether.fpsmeter 2>/dev/null || \
    dumpsys deviceidle whitelist +com.aether.fpsmeter 2>/dev/null || true
    cmd deviceidle whitelist +com.aether.engine.service 2>/dev/null || \
    dumpsys deviceidle whitelist +com.aether.engine.service 2>/dev/null || true
    unset _apk
fi

# Ensure Aether Engine Quick Settings Tile Provider APK is present
if [ -f "$MODDIR/system/app/AetherEngineCore/AetherEngineCore.apk" ] || [ -f "$MODDIR/tools/AetherEngineCore.apk" ]; then
    _qs_apk="$MODDIR/system/app/AetherEngineCore/AetherEngineCore.apk"
    [ ! -f "$_qs_apk" ] && _qs_apk="$MODDIR/tools/AetherEngineCore.apk"

    if ! pm path com.aether.engine.service >/dev/null 2>&1; then
        pm install -r -d -g "$_qs_apk" >/dev/null 2>&1 || true
    fi
    unset _qs_apk
fi

echo "[AETHER-KERNEL-OK] Pure In-Kernel Architecture active (0 userspace daemons)" > /data/adb/aether/engine_status.log

# ── v1.2 Pure In-Kernel RAM Calibration (Zero Polling, Silky Smooth) ──
# 1. Swappiness: 100 (kswapd handles ZRAM asynchronously without thrashing)
echo 100 > /proc/sys/vm/swappiness 2>/dev/null || true

# 2. Direct VM Swappiness: 20 (v1.2 golden ratio: stops UI threads from stalling on direct ZRAM swap!)
[ -f /proc/sys/vm/direct_vm_swappiness ] && echo 20 > /proc/sys/vm/direct_vm_swappiness 2>/dev/null || true

# 3. VFS Cache Pressure: 100 (Balanced 1:1 dentry/inode cache reclaim)
echo 100 > /proc/sys/vm/vfs_cache_pressure 2>/dev/null || true

# 4. Watermarks: 200 (2.0% reserve wakes kswapd early, avoiding direct reclaim stalls)
echo 200 > /proc/sys/vm/watermark_scale_factor 2>/dev/null || true

# 5. Free headroom
echo 16384 > /proc/sys/vm/min_free_kbytes 2>/dev/null || true
echo 0 > /proc/sys/vm/extra_free_kbytes 2>/dev/null || true

# 6. Page Cluster & Compaction
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null || true
echo 20 > /proc/sys/vm/compaction_proactiveness 2>/dev/null || true
echo 0 > /proc/sys/vm/compact_unevictable_allowed 2>/dev/null || true
echo 0 > /proc/sys/vm/percpu_pagelist_fraction 2>/dev/null || true
echo 524288 > /proc/sys/vm/max_map_count 2>/dev/null || true
echo 10 > /proc/sys/vm/stat_interval 2>/dev/null || true

# 7. Dirty Writeback
echo 10 > /proc/sys/vm/dirty_background_ratio 2>/dev/null || true
echo 20 > /proc/sys/vm/dirty_ratio 2>/dev/null || true
echo 500 > /proc/sys/vm/dirty_writeback_centisecs 2>/dev/null || true
echo 1500 > /proc/sys/vm/dirty_expire_centisecs 2>/dev/null || true

# 8. MGLRU: Multigenerational LRU (In-kernel working set protection)
if [ -f /sys/kernel/mm/lru_gen/enabled ]; then
    echo 7 > /sys/kernel/mm/lru_gen/enabled 2>/dev/null || echo y > /sys/kernel/mm/lru_gen/enabled 2>/dev/null || true
    echo 1000 > /sys/kernel/mm/lru_gen/min_ttl_ms 2>/dev/null || true
fi

# 9. ZRAM Queue Optimization (0 read-ahead, affinity 2, no merges, no iostats)
for zq in /sys/block/zram0/queue; do
    if [ -d "$zq" ]; then
        echo 0 > "$zq/read_ahead_kb" 2>/dev/null || true
        echo 2 > "$zq/rq_affinity" 2>/dev/null || true
        echo 2 > "$zq/nomerges" 2>/dev/null || true
        echo 0 > "$zq/iostats" 2>/dev/null || true
        echo 0 > "$zq/add_random" 2>/dev/null || true
    fi
done

# Background Storage & Cache Maintenance
(
    sleep 35
    # Only remove AETHER-specific temp files (never wipe all of /data/local/tmp - apps use it!)
    rm -f /data/local/tmp/AetherEngineCore.apk /data/local/tmp/AetherFpsMeter.apk /data/local/tmp/aether_fps 2>/dev/null || true
) &

# Detect Android version for version-specific compatibility guards
# Supports both API level (Android 17 = 36+, Android 16 = 35) and release string (17 vs 16)
SDK_VER=$(getprop ro.build.version.sdk 2>/dev/null || echo 34)
REL_VER=$(getprop ro.build.version.release 2>/dev/null || echo 14)
IS_A17=0
if [ "${SDK_VER:-34}" -ge 36 ] 2>/dev/null || [ "${REL_VER%%.*}" -ge 17 ] 2>/dev/null; then
    IS_A17=1
fi

# Ensure open node permissions for display, kernel, and battery control
chmod 0666 /sys/kernel/aether/* 2>/dev/null || true
chmod 0755 /sys/devices/platform/soc/soc:qcom,dsi-display 2>/dev/null || true
chmod 0666 /sys/devices/platform/soc/soc:qcom,dsi-display/* 2>/dev/null || true
chmod 0666 /sys/class/drm/card0-DSI-1/* 2>/dev/null || true
chmod 0666 /sys/class/drm/card0/sde-crtc-*/* 2>/dev/null || true
chmod 0666 /sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/sde-crtc-*/* 2>/dev/null || true
for crtc_p in /sys/class/drm/card0/sde-crtc-*/fps_periodicity /sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/sde-crtc-*/fps_periodicity; do
    [ -f "$crtc_p" ] && echo 300 > "$crtc_p" 2>/dev/null || true
done
for chg_node in \
    /sys/class/power_supply/battery/charging_enabled \
    /sys/class/power_supply/battery/battery_charging_enabled \
    /sys/class/power_supply/battery/constant_charge_current_max \
    /sys/class/power_supply/battery/fastcharge_mode
do
    [ -f "$chg_node" ] && chmod 0666 "$chg_node" 2>/dev/null || true
done
echo 1 > /sys/class/power_supply/battery/fastcharge_mode 2>/dev/null || true
echo 1 > /sys/class/power_supply/bms/fastcharge_mode 2>/dev/null || true

# Hardware Charge Cutoff & Bypass Persistence (Offline & Normal Boot)
if [ -f /data/adb/aether/cutoff.conf ]; then
    . /data/adb/aether/cutoff.conf 2>/dev/null
fi
if [ -f /mnt/vendor/persist/aether/cutoff.conf ]; then
    . /mnt/vendor/persist/aether/cutoff.conf 2>/dev/null
fi
# Default to 100% cutoff limit and mode 1 (real bypass hold) if not configured
[ -z "$CUTOFF_LIMIT" ] && CUTOFF_LIMIT="100"
[ -z "$CUTOFF_MODE" ] && CUTOFF_MODE="1"

echo "$CUTOFF_LIMIT" > /sys/kernel/aether/charge_cutoff_limit 2>/dev/null || true
echo "$CUTOFF_MODE" > /sys/kernel/aether/charge_cutoff_mode 2>/dev/null || true
[ -n "$INSTANT_BYPASS" ] && echo "$INSTANT_BYPASS" > /sys/kernel/aether/instant_bypass 2>/dev/null || true
[ -n "$SCREEN_BOOST" ] && echo "$SCREEN_BOOST" > /sys/kernel/aether/aacc_screen_boost 2>/dev/null || true
# Only write non-zero charge limits — writing 0 should mean stock unlimited
[ -n "$CHARGE_LIMIT" ] && [ "$CHARGE_LIMIT" != "0" ] && echo "$CHARGE_LIMIT" > /sys/kernel/aether/charge_current_limit 2>/dev/null || true


# ── Userspace Framework Logging & Debug Optimization ─────────────
# Applied once at boot completion

# 1. Logcat & Debug Logging Buffer Bounds
logcat -G 256K 2>/dev/null || true
resetprop -n persist.logd.size 262144 2>/dev/null || setprop persist.logd.size 262144 2>/dev/null || true
resetprop -n persist.logd.size.main 262144 2>/dev/null || setprop persist.logd.size.main 262144 2>/dev/null || true
resetprop -n persist.logd.size.system 131072 2>/dev/null || setprop persist.logd.size.system 131072 2>/dev/null || true
resetprop -n persist.logd.size.radio 131072 2>/dev/null || setprop persist.logd.size.radio 131072 2>/dev/null || true
resetprop -n persist.logd.size.crash 262144 2>/dev/null || setprop persist.logd.size.crash 262144 2>/dev/null || true

# 2. Disable debug tracing & StrictMode overhead
# ANDROID 17 FIX: Do NOT disable traced on A17 (IS_A17=1) — it breaks SensorService,
# auto-rotate, auto-brightness, screen recording, and MediaCodec on Android 17+.
if [ "$IS_A17" -eq 0 ]; then
    resetprop -n persist.traced.enable 0 2>/dev/null || setprop persist.traced.enable 0 2>/dev/null || true
else
    resetprop --delete persist.traced.enable 2>/dev/null || true
fi
resetprop -n persist.sys.assert.enable false 2>/dev/null || setprop persist.sys.assert.enable false 2>/dev/null || true
resetprop -n persist.sys.strictmode.visual 0 2>/dev/null || setprop persist.sys.strictmode.visual 0 2>/dev/null || true
resetprop --delete persist.libc.debug.malloc 2>/dev/null || true
resetprop --delete libc.debug.malloc 2>/dev/null || true

# 6. Background CPUset confinement (keep low-priority work on Silver cores 0-5)
if [ -d /dev/cpuset/background ]; then
    echo "0-5" > /dev/cpuset/background/cpus 2>/dev/null || true
fi
if [ -d /dev/cpuset/system-background ]; then
    echo "0-5" > /dev/cpuset/system-background/cpus 2>/dev/null || true
fi

# 7. RenderThread & SurfaceFlinger Safety Cleanups
# CRITICAL FIX: debug.sf.disable_backpressure and latch_unsignaled break screen recording
# and cause linear block allocation crashes in c2.qti.avc.encoder. Always delete them!
# NOTE: persist.traced.enable is intentionally NOT deleted here — it is already handled
# conditionally above (set=0 on Android <17, deleted on Android 17+) at the IS_A17 block.
resetprop --delete debug.sf.disable_backpressure 2>/dev/null || true
resetprop --delete debug.sf.latch_unsignaled 2>/dev/null || true
resetprop --delete debug.hwui.render_thread_priority 2>/dev/null || true
resetprop --delete renderthread.skia.reduceopstasksplitting 2>/dev/null || true

# 8. SchedTune Top-App & CPU Input Boost (Default EAS: Big cores untouched, no sched migration)
if [ -d /dev/stune/top-app ]; then
    echo 1 > /dev/stune/top-app/schedtune.prefer_idle 2>/dev/null || true
    echo 0 > /dev/stune/top-app/schedtune.colocate 2>/dev/null || true
    echo 0 > /dev/stune/top-app/schedtune.boost 2>/dev/null || true
fi
for cb in /sys/module/cpu_boost/parameters /sys/module/cpu_input_boost/parameters; do
    if [ -d "$cb" ]; then
        echo "0:1017600 1:1017600 2:1017600 3:1017600 4:1017600 5:1017600 6:0 7:0" > "$cb/input_boost_freq" 2>/dev/null || true
        echo 40 > "$cb/input_boost_ms" 2>/dev/null || true
        echo 40 > "$cb/input_boost_duration" 2>/dev/null || true
        echo 0 > "$cb/sched_boost_on_input" 2>/dev/null || true
    fi
done
# 9. Deep Sleep & Idle Drain Protections (EAS, C-State Aware, LPM Prediction)
# Note: Timer migration is natively 0 in kernel/time/timer.c (prevents timer wake storms)
# Note: Freeze timeout is natively 1500ms in kernel/power/process.c (instant deep sleep)
echo 1 > /proc/sys/kernel/sched_energy_aware 2>/dev/null || true
echo 0 > /proc/sys/kernel/sched_walt_rotate_big_tasks 2>/dev/null || true
echo 1 > /proc/sys/kernel/sched_cstate_aware 2>/dev/null || true
echo 1 > /sys/module/workqueue/parameters/power_efficient 2>/dev/null || true
echo 1 > /sys/module/printk/parameters/console_suspend 2>/dev/null || true
echo "4 4 1 7" > /proc/sys/kernel/printk 2>/dev/null || true
echo 1 > /sys/power/pm_async 2>/dev/null || true

# Qualcomm LPM & C-State Deep Sleep (AOSS / RPM Power Collapse with reliable WLAN)
echo 0 > /sys/module/lpm_levels/parameters/sleep_disabled 2>/dev/null || true
echo 1 > /sys/module/lpm_levels/parameters/lpm_prediction 2>/dev/null || true
echo 1 > /sys/module/lpm_levels/parameters/lpm_ipi_prediction 2>/dev/null || true

# Android Balanced Deep Sleep / Doze (Only tune inactive timeouts on Android <= 16; do not touch sensors)
if [ "$IS_A17" -eq 0 ]; then
    cmd device_config put device_idle light_after_inactive_to 300000 2>/dev/null || true
    cmd device_config put device_idle inactive_to 600000 2>/dev/null || true
fi

echo 1 > /sys/module/kprofiles/parameters/auto_kp 2>/dev/null || true
echo 0 > /sys/kernel/kprofiles/kp_mode 2>/dev/null || true

# 10. Android Memory Props & Userspace LMKD Sanitization
resetprop -n persist.sys.zram_enabled 1 2>/dev/null || setprop persist.sys.zram_enabled 1 2>/dev/null || true

# ── Android 17 Critical Boot Properties ──────────────────────────────────
# CONFIRMED from real device testing: NOT setting these on Android 17 causes bootloop.
# Android 17 removed /dev/ashmem fallback — Zygote REQUIRES memfd for shared memory.
# These props must be set AFTER sys.boot_completed=1 to avoid racing with init.
if [ "$IS_A17" -ge 1 ] 2>/dev/null; then
    # 1. sys.use_memfd [REQUIRED on A17]
    #    Zygote uses this to choose memfd_create() over /dev/ashmem.
    #    Without it on A17: ashmem open fails → Zygote abort → bootloop.
    resetprop sys.use_memfd 1 2>/dev/null || setprop sys.use_memfd 1 2>/dev/null || true

    # 2. dalvik.vm.heapsize validation [REQUIRED on A17]
    #    A17 ART validates heap props at Zygote fork. Missing or malformed values
    #    cause ART to crash with "Invalid -Xmx" during secondary Zygote start.
    resetprop -n dalvik.vm.heapstartsize 8m 2>/dev/null || true
    resetprop -n dalvik.vm.heapgrowthlimit 192m 2>/dev/null || true
    resetprop -n dalvik.vm.heapsize 512m 2>/dev/null || true
    resetprop -n dalvik.vm.heapminfree 512k 2>/dev/null || true
    resetprop -n dalvik.vm.heapmaxfree 8m 2>/dev/null || true
    resetprop -n dalvik.vm.heaptargetutilization 0.75 2>/dev/null || true

    # 3. Binder memfd compat [REQUIRED on A17]
    #    Android 17 binder driver uses memfd for large transactions.
    #    These prevent EBADF crashes in system_server's binder thread pool.
    resetprop -n persist.sys.binder.memfd_mode 1 2>/dev/null || true

    # 4. HWC2 / SurfaceFlinger fence timeout [SAFETY on A17]
    #    A17 changed HWC2 fence timeout defaults. Short timeouts cause SF to
    #    abort() during early display bring-up before VSYNC is stable.
    resetprop -n debug.sf.presentTimeOffset 0 2>/dev/null || true
    resetprop --delete debug.sf.early_phase_offset_ns 2>/dev/null || true
    resetprop --delete debug.sf.earlyGl_phase_offset_ns 2>/dev/null || true

    # 5. Vendor service restart guard [SAFETY on A17]
    #    Prevents vendor HAL watchdog from force-restarting system_server
    #    during the first 30 seconds when kernel tweaks are being applied.
    resetprop -n ro.vendor.qti.sys.fw.bg_apps_limit 60 2>/dev/null || true

    # 6. SELinux avc denial guard [SAFETY on A17]
    #    Prevent permissive-mode policy from being forcibly disabled during
    #    module service execution (some A17 ROMs enforce this at boot).
    resetprop --delete persist.selinux.policy_version 2>/dev/null || true
else
    # Android < 17: sys.use_memfd is controlled by ROM init — do not touch it.
    # Overriding on older Android can break ashmem-based legacy apps.
    true
fi

# Userspace LMKD: Preserved ROM vendor calibration for Android 17 stability

# 11. Adreno 612 GPU Baseline
if [ -d /sys/class/kgsl/kgsl-3d0 ]; then
    echo "msm-adreno-tz" > /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null || true
    echo 290 > /sys/class/kgsl/kgsl-3d0/min_clock_mhz 2>/dev/null || true
    echo 290000000 > /sys/class/kgsl/kgsl-3d0/devfreq/min_freq 2>/dev/null || true
    [ -f /sys/class/kgsl/kgsl-3d0/min_pwrlevel ] && echo 5 > /sys/class/kgsl/kgsl-3d0/min_pwrlevel 2>/dev/null || true
    echo 64 > /sys/class/kgsl/kgsl-3d0/idle_timer 2>/dev/null || true
    # Critical: allow GPU to power-collapse to true idle (0mA) when not in use.
    # If these are 1, the GPU rail stays powered even at 0% load (wastes ~30-80mA).
    echo 0 > /sys/class/kgsl/kgsl-3d0/force_bus_on 2>/dev/null || true
    echo 0 > /sys/class/kgsl/kgsl-3d0/force_clk_on 2>/dev/null || true
    echo 0 > /sys/class/kgsl/kgsl-3d0/force_rail_on 2>/dev/null || true

    # GPU User Preference Persistence
    if [ -f /data/adb/aether/gpu.conf ]; then
        . /data/adb/aether/gpu.conf 2>/dev/null
        [ -n "$GPU_GOV" ] && echo "$GPU_GOV" > /sys/class/kgsl/kgsl-3d0/devfreq/governor 2>/dev/null || true
        [ -n "$GPU_GOV" ] && [ -f /sys/class/kgsl/kgsl-3d0/gpu_governor ] && echo "$GPU_GOV" > /sys/class/kgsl/kgsl-3d0/gpu_governor 2>/dev/null || true
        if [ -n "$GPU_MIN_MHZ" ]; then
            echo "$GPU_MIN_MHZ" > /sys/class/kgsl/kgsl-3d0/min_clock_mhz 2>/dev/null || true
            echo "${GPU_MIN_MHZ}000000" > /sys/class/kgsl/kgsl-3d0/devfreq/min_freq 2>/dev/null || true
            case "$GPU_MIN_MHZ" in
                845) pwr=0 ;;
                745) pwr=1 ;;
                700) pwr=2 ;;
                550) pwr=3 ;;
                435) pwr=4 ;;
                290|*) pwr=5 ;;
            esac
            [ -f /sys/class/kgsl/kgsl-3d0/min_pwrlevel ] && echo "$pwr" > /sys/class/kgsl/kgsl-3d0/min_pwrlevel 2>/dev/null || true
        fi
        [ -n "$GPU_IDLE" ] && echo "$GPU_IDLE" > /sys/class/kgsl/kgsl-3d0/idle_timer 2>/dev/null || true
    fi
fi

# 12. Network & TCP Bufferbloat Mitigation Stack
echo "fq" > /proc/sys/net/core/default_qdisc 2>/dev/null || true
echo "bbr" > /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null || true
echo 3 > /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null || true
echo 0 > /proc/sys/net/ipv4/tcp_slow_start_after_idle 2>/dev/null || true
echo 2 > /proc/sys/net/ipv4/tcp_ecn 2>/dev/null || true
echo 16384 > /proc/sys/net/ipv4/tcp_notsent_lowat 2>/dev/null || true
echo 1 > /proc/sys/net/ipv4/tcp_autocorking 2>/dev/null || true
echo 2097152 > /proc/sys/net/core/rmem_max 2>/dev/null || true
echo 2097152 > /proc/sys/net/core/wmem_max 2>/dev/null || true
echo "4096 87380 2097152" > /proc/sys/net/ipv4/tcp_rmem 2>/dev/null || true
echo "4096 65536 2097152" > /proc/sys/net/ipv4/tcp_wmem 2>/dev/null || true

# Purge any legacy generic override scripts to guarantee dev-set kernel values are never overwritten
rm -f /data/adb/aether/aether_tweaks.sh 2>/dev/null || true

# If the user explicitly customized and saved personalized tweaks, re-apply them once on boot
if [ -f /data/adb/aether/aether_user_tweaks.sh ]; then
    sh /data/adb/aether/aether_user_tweaks.sh >/dev/null 2>&1 || true
fi

# 13. UFS Host Controller Hardware Power Management (SM6150 1d84000.ufshc)
UFS=/sys/devices/platform/soc/1d84000.ufshc
if [ -d "$UFS" ]; then
    echo 15 > "$UFS/clkgate_delay_ms_perf"      2>/dev/null || true
    echo 15 > "$UFS/clkgate_delay_ms"            2>/dev/null || true
    echo 8  > "$UFS/clkgate_delay_ms_pwr_save"   2>/dev/null || true
    echo 5000 > "$UFS/auto_hibern8"              2>/dev/null || true
    echo 1 > "$UFS/rpm_lvl"                      2>/dev/null || true
    echo 3 > "$UFS/spm_lvl"                      2>/dev/null || true
    echo 1  > "$UFS/clk_scaling/enable"          2>/dev/null || true
    echo 30 > "$UFS/clk_scaling/scale_up_threshold"   2>/dev/null || true
    echo 20 > "$UFS/clk_scaling/scale_down_threshold" 2>/dev/null || true
    echo 1  > "$UFS/wb_on"                       2>/dev/null || true
    echo 1  > "$UFS/wb_buf_flush_en"             2>/dev/null || true
    echo 80 > "$UFS/wb_buf_flush_thresh"         2>/dev/null || true
    echo 1  > "$UFS/wb_flush_stat_after_h8"      2>/dev/null || true
    echo 1  > "$UFS/write_booster_enable"        2>/dev/null || true
    echo 5  > "$UFS/hibern8_on_idle_delay"       2>/dev/null || true
fi
unset UFS

exit 0

