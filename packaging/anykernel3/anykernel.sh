### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers
## Customized for AETHER Kernel by Kiran_Embedded (https://github.com/kiran-embedded)

### AnyKernel setup
# begin properties
properties() { '
kernel.string=AETHER Kernel v1.4 [Enhanced] by Kiran_Embedded
do.devicecheck=1
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=mojito
device.name2=sunny
supported.versions=
supported.patchlevels=
'; } # end properties

### AnyKernel install
# begin attributes
attributes() {
set_perm_recursive 0 0 755 644 $ramdisk/*;
set_perm_recursive 0 0 750 750 $ramdisk/init* $ramdisk/sbin;
} # end attributes

## boot shell variables
block=/dev/block/bootdevice/by-name/boot;
is_slot_device=auto;
ramdisk_compression=auto;
patch_vbmeta_flag=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh && attributes;

# -------------------------------------------------------------
# 1. Device Safety Verification & Universal Protection
# -------------------------------------------------------------
dev=$(getprop ro.product.device 2>/dev/null);
[ -z "$dev" ] && dev=$(getprop ro.build.product 2>/dev/null);
[ -z "$dev" ] && dev=$(getprop ro.product.vendor.device 2>/dev/null);
[ -z "$dev" ] && dev=$(getprop ro.vendor.product.device 2>/dev/null);

model=$(getprop ro.product.model 2>/dev/null);
brand=$(getprop ro.product.brand 2>/dev/null);
marketname=$(getprop ro.product.marketname 2>/dev/null);

# Inspect system build.prop for complete model info if recovery props are sparse
for p in /system_root/system/build.prop /system/build.prop /system/system/build.prop /vendor/build.prop /product/build.prop; do
  if [ -f "$p" ]; then
    [ -z "$dev" ] && dev=$(file_getprop "$p" ro.product.device 2>/dev/null);
    [ -z "$model" ] && model=$(file_getprop "$p" ro.product.model 2>/dev/null);
    [ -z "$brand" ] && brand=$(file_getprop "$p" ro.product.brand 2>/dev/null);
    [ -z "$marketname" ] && marketname=$(file_getprop "$p" ro.product.marketname 2>/dev/null);
    [ "$dev" -a "$model" ] && break;
  fi;
done;

# Construct friendly device name dynamically
detected_device_name="${marketname:-${brand} ${model}}";
[ -z "$detected_device_name" -o "$detected_device_name" = " " ] && detected_device_name="${model:-Unknown Device}";

# Strict Universal Whitelist: ONLY allow mojito and sunny
case "$dev" in
  mojito*|sunny*)
    dev_display="Redmi Note 10 ($dev)";
    ;;
  *)
    ui_print " ";
    ui_print "  ╔══════════════════════════════════════════════╗";
    ui_print "  ║              CRITICAL SAFETY ERROR           ║";
    ui_print "  ╠══════════════════════════════════════════════╣";
    ui_print "  ║  Incompatible device detected:               ║";
    ui_print "  ║  Model   : $detected_device_name";
    ui_print "  ║  Codename: ${dev:-Unknown}";
    ui_print "  ║                                              ║";
    ui_print "  ║  This kernel is built strictly for:          ║";
    ui_print "  ║  Xiaomi Redmi Note 10 (mojito / sunny)       ║";
    ui_print "  ║  Platform: Qualcomm Snapdragon 678 (SM6150)  ║";
    ui_print "  ║                                              ║";
    ui_print "  ║  Flashing on '$dev' will cause a hard brick! ║";
    ui_print "  ║  Installation aborted safely.                ║";
    ui_print "  ╚══════════════════════════════════════════════╝";
    ui_print " ";
    exit 1;
    ;;
esac

# -------------------------------------------------------------
# 2. Dynamic Installed ROM & Android Detection (No Hardcoding)
# -------------------------------------------------------------
rom_display="AOSP / Custom ROM";
android_display="11 - 15";
patch_display="Current";
slot_display="Active";

# Detect active boot slot
slot=$(getprop ro.boot.slot_suffix 2>/dev/null);
[ -z "$slot" ] && slot=$(grep -o 'androidboot.slot_suffix=.*$' /proc/cmdline 2>/dev/null | cut -d\  -f1 | cut -d= -f2);
[ -z "$slot" ] && slot=$(getprop ro.boot.slot 2>/dev/null);
case "$slot" in
  _a|a|A) slot_display="Slot A" ;;
  _b|b|B) slot_display="Slot B" ;;
  *)      slot_display="Standard" ;;
esac

# Attempt to inspect system build.prop
for candidate in \
  /system_root/system/build.prop \
  /system/build.prop \
  /system/system/build.prop \
  /vendor/build.prop \
  /product/build.prop \
  /product/etc/build.prop
do
  if [ -f "$candidate" ]; then
    bp="$candidate";
    break;
  fi;
done

if [ "$bp" ]; then
  # Android Version & Security Patch
  a_ver=$(file_getprop "$bp" ro.build.version.release 2>/dev/null);
  [ "$a_ver" ] && android_display="Android $a_ver";
  s_patch=$(file_getprop "$bp" ro.build.version.security_patch 2>/dev/null);
  [ "$s_patch" ] && patch_display="$s_patch";

  # Custom ROM Identifiers
  lineage_v=$(file_getprop "$bp" ro.lineage.version 2>/dev/null || file_getprop "$bp" ro.lineage.build.version 2>/dev/null);
  crdroid_v=$(file_getprop "$bp" ro.crdroid.version 2>/dev/null);
  evo_v=$(file_getprop "$bp" ro.evolution.version 2>/dev/null);
  pixelos_v=$(file_getprop "$bp" ro.pixelos.version 2>/dev/null);
  pixys_v=$(file_getprop "$bp" ro.pixys.version 2>/dev/null);
  arrow_v=$(file_getprop "$bp" ro.arrow.version 2>/dev/null);
  rising_v=$(file_getprop "$bp" ro.rising.version 2>/dev/null);
  matrixx_v=$(file_getprop "$bp" ro.matrixx.version 2>/dev/null);
  derp_v=$(file_getprop "$bp" ro.derp.version 2>/dev/null);
  spark_v=$(file_getprop "$bp" ro.spark.version 2>/dev/null);
  corvus_v=$(file_getprop "$bp" ro.corvus.version 2>/dev/null);
  miui_v=$(file_getprop "$bp" ro.miui.ui.version.name 2>/dev/null || file_getprop "$bp" ro.mi.os.version.name 2>/dev/null);
  mod_v=$(file_getprop "$bp" ro.modversion 2>/dev/null);
  disp_id=$(file_getprop "$bp" ro.build.display.id 2>/dev/null);

  if [ "$lineage_v" ]; then
    rom_display="LineageOS $lineage_v";
  elif [ "$crdroid_v" ]; then
    rom_display="crDroid $crdroid_v";
  elif [ "$evo_v" ]; then
    rom_display="Evolution X $evo_v";
  elif [ "$pixelos_v" ]; then
    rom_display="PixelOS $pixelos_v";
  elif [ "$pixys_v" ]; then
    rom_display="PixysOS $pixys_v";
  elif [ "$rising_v" ]; then
    rom_display="RisingOS $rising_v";
  elif [ "$matrixx_v" ]; then
    rom_display="Project Matrixx $matrixx_v";
  elif [ "$derp_v" ]; then
    rom_display="DerpFest $derp_v";
  elif [ "$arrow_v" ]; then
    rom_display="ArrowOS $arrow_v";
  elif [ "$spark_v" ]; then
    rom_display="SparkOS $spark_v";
  elif [ "$corvus_v" ]; then
    rom_display="CorvusOS $corvus_v";
  elif [ "$miui_v" ]; then
    rom_display="MIUI / HyperOS ($miui_v)";
  elif [ "$mod_v" ]; then
    rom_display="$mod_v";
  elif [ "$disp_id" ]; then
    rom_display="$disp_id";
  fi;
fi;

# -------------------------------------------------------------
# 3. Clean Installation Banner & Subsystem Overview
# -------------------------------------------------------------
ui_print " ";
ui_print "  ╔══════════════════════════════════════════════╗";
ui_print "  ║                 A E T H E R                  ║";
ui_print "  ║             Custom Linux Kernel              ║";
ui_print "  ╠══════════════════════════════════════════════╣";
ui_print "  ║  Version  : v1.4                             ║";
ui_print "  ║  Device   : $dev_display";
ui_print "  ║  Platform : Snapdragon 678 (SM6150)          ║";
ui_print "  ║  System   : $rom_display";
ui_print "  ║  Android  : $android_display ($patch_display)";
ui_print "  ║  Target   : $slot_display";
ui_print "  ║  Edition  : Enhanced (Calibrated)            ║";
ui_print "  ╠══════════════════════════════════════════════╣";
ui_print "  ║  Kernel Features:                            ║";
ui_print "  ║   • CPU Sched : BORE v5.1.0 + Energy Schedutil ║";
ui_print "  ║   • Boost     : Sultan CPU & Devfreq Boost   ║";
ui_print "  ║   • Memory    : Multi-Gen LRU (MGLRU)        ║";
ui_print "  ║   • Root      : KernelSU-Next + SUSFS v2.3.0 ║";
ui_print "  ║   • Storage   : F2FS Rapid Discard & UFS I/O ║";
ui_print "  ║   • Display   : DC Dimming & Sunlight HBM    ║";
ui_print "  ║   • Network   : BBR + FQ Congestion Control  ║";
ui_print "  ╠══════════════════════════════════════════════╣";
ui_print "  ║  Credits & Attribution:                      ║";
ui_print "  ║   • osm0sis     : AnyKernel3 installer       ║";
ui_print "  ║   • Yu Zhao     : Multi-Gen LRU (MGLRU)      ║";
ui_print "  ║   • mascotski   : BORE CPU scheduler         ║";
ui_print "  ║   • P. Zijlstra : Linux core scheduler       ║";
ui_print "  ║   • S. Alsawaf  : CPU & devfreq boost        ║";
ui_print "  ║   • J. Axboe    : Linux block I/O layer      ║";
ui_print "  ║   • J. Kim      : F2FS filesystem            ║";
ui_print "  ║   • tiann       : KernelSU                   ║";
ui_print "  ║   • rifsxd      : KernelSU-Next              ║";
ui_print "  ║   • backslashxx : Non-GKI architecture       ║";
ui_print "  ║   • simonpunk   : SUSFS stealth filesystem   ║";
ui_print "  ║   • arter97     : Memory optimizations       ║";
ui_print "  ║   • Saxo06      : SUSFS v2.3.0 & KSU patches ║";
ui_print "  ║   • Author      : Kiran_Embedded             ║";
ui_print "  ╚══════════════════════════════════════════════╝";
ui_print " ";

# -------------------------------------------------------------
# 4. Step-by-Step Installation & Bootloop Protection
# -------------------------------------------------------------

# Step 1: Device compatibility check
ui_print "  [1/5] Verifying device safety & hardware target...";
sleep 0.2;
ui_print "        [██░░░░░░░░]  20%  Hardware confirmed: $dev_display";
ui_print " ";

# Step 2: System inspection
ui_print "  [2/5] Inspecting installed ROM & environment...";
sleep 0.2;
ui_print "        [████░░░░░░]  40%  Target: $rom_display ($android_display)";
ui_print " ";

# Step 3: Boot partition backup & unpack
ui_print "  [3/5] Backing up and unpacking boot partition...";
if [ ! -e "$(echo $block | cut -d\  -f1)" ]; then
  abort "Error: Target boot partition ($block) not found! Aborting.";
fi;

# Safety backup of existing boot partition for recovery
dd if=$block of=/tmp/boot_stock_backup.img bs=1M 2>/dev/null;
[ -d /sdcard ] && cp -f /tmp/boot_stock_backup.img /sdcard/AETHER_boot_backup.img 2>/dev/null;
ui_print "        └─ Safety backup created (/sdcard/AETHER_boot_backup.img)";

dump_boot;
sleep 0.2;
ui_print "        [██████░░░░]  60%  Boot image unpacked cleanly";
ui_print " ";

# Patch ramdisk for offline charger persistence
for f in $ramdisk/init.rc $ramdisk/init.qcom.rc $ramdisk/init.target.rc; do
    if [ -f "$f" ] && ! grep -q "aether_cutoff" "$f"; then
        cat << 'EOF' >> "$f"

# AETHER In-Kernel Hardware Charge Cutoff Persistence (Offline Charger Mode)
on property:ro.bootmode=charger
    exec - root root -- /system/bin/sh -c "[ -f /mnt/vendor/persist/aether/cutoff.conf ] && . /mnt/vendor/persist/aether/cutoff.conf && [ -n \"$CUTOFF_LIMIT\" ] && echo \"$CUTOFF_LIMIT\" > /sys/kernel/aether/charge_cutoff_limit && [ -n \"$CUTOFF_MODE\" ] && echo \"$CUTOFF_MODE\" > /sys/kernel/aether/charge_cutoff_mode"

on charger
    exec - root root -- /system/bin/sh -c "[ -f /mnt/vendor/persist/aether/cutoff.conf ] && . /mnt/vendor/persist/aether/cutoff.conf && [ -n \"$CUTOFF_LIMIT\" ] && echo \"$CUTOFF_LIMIT\" > /sys/kernel/aether/charge_cutoff_limit && [ -n \"$CUTOFF_MODE\" ] && echo \"$CUTOFF_MODE\" > /sys/kernel/aether/charge_cutoff_mode"
EOF
    fi
done

# Step 4: Write Kernel Image & DTB
ui_print "  [4/5] Repacking and writing kernel image...";
sleep 0.2;
write_boot;
ui_print "        └─ Boot partition repacked cleanly (Stock DTBO protected)";
ui_print "        [████████░░]  80%  Kernel image and device tree applied";
ui_print " ";

# Step 5: Runtime Configuration & Optimization
ui_print "  [5/5] Configuring runtime services & nodes...";
mount /data 2>/dev/null;
if [ -d /data/adb ]; then
    mkdir -p /data/adb/ksu/bin /data/adb/service.d /data/adb/post-fs-data.d;
    if [ -f "$home/tools/ksud" ]; then
        cp -f "$home/tools/ksud" /data/adb/ksu/bin/ksud;
        chmod 0755 /data/adb/ksu/bin/ksud;
        chown 0:0 /data/adb/ksu/bin/ksud;
        ui_print "        └─ KernelSU binary initialized";
    fi;
    if [ -f "$home/tools/ksu_susfs" ]; then
        cp -f "$home/tools/ksu_susfs" /data/adb/ksu/bin/ksu_susfs;
        chmod 0755 /data/adb/ksu/bin/ksu_susfs;
        chown 0:0 /data/adb/ksu/bin/ksu_susfs;
        ui_print "        └─ SUSFS binary initialized";
    fi;

    # Purge legacy throttled charging overrides from previous flashes/scripts
    if [ -f /data/adb/service.d/01_displayfeatures.sh ]; then
        rm -f /data/adb/service.d/01_displayfeatures.sh
    fi
    if [ -f /data/adb/aether/aether_tweaks.sh ]; then
        sed -i '/constant_charge_current_max/d' /data/adb/aether/aether_tweaks.sh 2>/dev/null || true
        sed -i '/step_charging_enabled/d' /data/adb/aether/aether_tweaks.sh 2>/dev/null || true
        sed -i '/battery_charging_enabled/d' /data/adb/aether/aether_tweaks.sh 2>/dev/null || true
        sed -i '/fastcharge_mode/d' /data/adb/aether/aether_tweaks.sh 2>/dev/null || true
    fi

    # Service script for display nodes, I/O, network, and scheduler
    cat << 'EOF' > /data/adb/service.d/01_displayfeatures.sh
#!/system/bin/sh
# Display nodes permissions
for node in \
    /sys/devices/platform/soc/*/*/regulator/regulator.*/soc:qcom,dsi-display-lab/anti_flicker \
    /sys/devices/platform/soc/*/*/regulator/*/soc:qcom,dsi-display-lab/anti_flicker \
    /sys/devices/platform/soc/*/soc:qcom,dsi-display-lab/anti_flicker \
    /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/anti_flicker \
    /sys/devices/platform/soc/*/*/regulator/regulator.*/soc:qcom,dsi-display-lab/hbm \
    /sys/devices/platform/soc/*/*/regulator/*/soc:qcom,dsi-display-lab/hbm \
    /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/hbm \
    /sys/devices/platform/soc/soc:qcom,dsi-display-primary/hbm \
    /sys/devices/platform/soc/*/soc:qcom,dsi-display*/hbm \
    /sys/devices/virtual/graphics/fb0/anti_flicker \
    /sys/class/graphics/fb0/anti_flicker
do
    [ -f "$node" ] && chmod 0666 "$node" && chown system:system "$node" 2>/dev/null || true
done
fps_node="/sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/sde-crtc-0/measured_fps"
[ -f "$fps_node" ] && chmod 0664 "$fps_node" && chown system:graphics "$fps_node"

# Block queue and UFS read-ahead buffer tuning (Stage 1: 2048KB Boot Burst for Instant App & Dex Loading)
for q in /sys/block/sd*/queue /sys/block/dm-*/queue; do
    if [ -d "$q" ]; then
        if [ -f "$q/scheduler" ]; then
            grep -q "kyber" "$q/scheduler" 2>/dev/null && echo "kyber" > "$q/scheduler" 2>/dev/null || echo "mq-deadline" > "$q/scheduler" 2>/dev/null
        fi
        echo 0 > "$q/add_random" 2>/dev/null || true
        echo 0 > "$q/iostats" 2>/dev/null || true
        echo 0 > "$q/nomerges" 2>/dev/null || true
        echo 0 > "$q/rotational" 2>/dev/null || true
        echo 2 > "$q/rq_affinity" 2>/dev/null || true
        echo 256 > "$q/nr_requests" 2>/dev/null || true
        echo 2048 > "$q/read_ahead_kb" 2>/dev/null || true
    fi
done

# ZRAM swap block queue tuning (RAM-backed, zero readahead, affinity bound)
for zq in /sys/block/zram*/queue; do
    if [ -d "$zq" ]; then
        echo 0 > "$zq/read_ahead_kb" 2>/dev/null || true
        echo 2 > "$zq/rq_affinity" 2>/dev/null || true
        echo 2 > "$zq/nomerges" 2>/dev/null || true
        echo 0 > "$zq/add_random" 2>/dev/null || true
        echo 0 > "$zq/iostats" 2>/dev/null || true
    fi
done

# F2FS rapid asynchronous discard tuning & zero-lag GC pacing
for f2fs_disk in /sys/fs/f2fs/*; do
    if [ -d "$f2fs_disk" ]; then
        echo 0 > "$f2fs_disk/gc_urgent" 2>/dev/null || true
        echo 0 > "$f2fs_disk/iostat_enable" 2>/dev/null || true
        echo 18 > "$f2fs_disk/ipu_policy" 2>/dev/null || true
        echo 4 > "$f2fs_disk/min_fsync_blocks" 2>/dev/null || true
        echo 64 > "$f2fs_disk/max_discard_request" 2>/dev/null || true
        echo 5 > "$f2fs_disk/min_discard_issue_time" 2>/dev/null || true
        echo 50 > "$f2fs_disk/mid_discard_issue_time" 2>/dev/null || true
        echo 4 > "$f2fs_disk/discard_granularity" 2>/dev/null || true
    fi
done

# UFS clock gating delay (energy-efficient fast-gating 15ms perf / 10ms power save)
for ufs_clk in /sys/devices/platform/soc/*.ufshc /sys/bus/platform/drivers/ufshcd-qcom/*; do
    if [ -d "$ufs_clk" ]; then
        echo 15 > "$ufs_clk/clkgate_delay_ms_perf" 2>/dev/null || true
        echo 10 > "$ufs_clk/clkgate_delay_ms" 2>/dev/null || true
    fi
done

# Schedutil rate limits (Anti-Spike DVFS pacing: 500us up, 10ms down on Little cores, 5ms down on Big cores)
for pol in /sys/devices/system/cpu/cpufreq/policy*; do
    if [ -d "$pol/schedutil" ]; then
        echo 500 > "$pol/schedutil/up_rate_limit_us" 2>/dev/null || true
        echo 10000 > "$pol/schedutil/down_rate_limit_us" 2>/dev/null || true
    fi
done
if [ -d /sys/devices/system/cpu/cpufreq/policy6/schedutil ]; then
    echo 5000 > /sys/devices/system/cpu/cpufreq/policy6/schedutil/down_rate_limit_us 2>/dev/null || true
fi

# CPU & Devfreq Boost Tuning (Lightweight, responsive 40ms touch boost / energy-saving 60-80ms wake boost)
echo 40 > /sys/module/cpu_input_boost/parameters/input_boost_duration 2>/dev/null || true
echo 60 > /sys/module/cpu_input_boost/parameters/wake_boost_duration 2>/dev/null || true
echo 40 > /sys/module/devfreq_boost/parameters/devfreq_boost_duration 2>/dev/null || true
echo 80 > /sys/module/devfreq_boost/parameters/devfreq_wake_boost_duration 2>/dev/null || true

# Network & Web performance parameters (BBR + FQ, TFO 3, Low-Power Socket Lowat)
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

# Virtual Memory & RAM Optimization (Zero-stall writeback trickle, MGLRU retain, safety watermark)
echo 100 > /proc/sys/vm/vfs_cache_pressure 2>/dev/null || true
echo 60 > /proc/sys/vm/swappiness 2>/dev/null || true
echo 10 > /proc/sys/vm/dirty_background_ratio 2>/dev/null || true
echo 20 > /proc/sys/vm/dirty_ratio 2>/dev/null || true
echo 1000 > /proc/sys/vm/dirty_writeback_centisecs 2>/dev/null || true
echo 2000 > /proc/sys/vm/dirty_expire_centisecs 2>/dev/null || true
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null || true
echo 150 > /proc/sys/vm/watermark_scale_factor 2>/dev/null || true
if [ -f /sys/kernel/mm/lru_gen/enabled ]; then
    echo y > /sys/kernel/mm/lru_gen/enabled 2>/dev/null || true
    echo 1000 > /sys/kernel/mm/lru_gen/min_ttl_ms 2>/dev/null || true
fi

# Energy-Aware Scheduling & Low-Power Idle Modes (SoC Deep Sleep active)
echo 1 > /proc/sys/kernel/sched_energy_aware 2>/dev/null || true
echo 0 > /proc/sys/kernel/sched_walt_rotate_big_tasks 2>/dev/null || true
echo 1 > /sys/module/workqueue/parameters/power_efficient 2>/dev/null || true
echo 1 > /sys/module/printk/parameters/console_suspend 2>/dev/null || true
echo "4 4 1 7" > /proc/sys/kernel/printk 2>/dev/null || true
echo 0 > /sys/module/lpm_levels/parameters/sleep_disabled 2>/dev/null || true
echo 1 > /sys/module/lpm_levels/parameters/lpm_prediction 2>/dev/null || true
echo 1 > /sys/module/lpm_levels/parameters/lpm_ipi_prediction 2>/dev/null || true
if [ -d /dev/cpuset/top-app ]; then
    echo "0-7" > /dev/cpuset/top-app/cpus 2>/dev/null || true
fi
if [ -d /dev/cpuset/foreground ]; then
    echo "0-7" > /dev/cpuset/foreground/cpus 2>/dev/null || true
fi
if [ -d /dev/cpuset/background ]; then
    echo "0-3" > /dev/cpuset/background/cpus 2>/dev/null || true
fi
if [ -d /dev/cpuset/system-background ]; then
    echo "0-3" > /dev/cpuset/system-background/cpus 2>/dev/null || true
fi
if [ -d /dev/cpuset/restricted ]; then
    echo "0-2" > /dev/cpuset/restricted/cpus 2>/dev/null || true
fi
if [ -d /dev/stune/top-app ]; then
    echo 1 > /dev/stune/top-app/schedtune.prefer_idle 2>/dev/null || true
    echo 1 > /dev/stune/top-app/schedtune.boost 2>/dev/null || true
    echo 1 > /dev/stune/top-app/schedtune.colocate 2>/dev/null || true
    echo 1 > /dev/stune/top-app/schedtune.sched_boost_no_override 2>/dev/null || true
fi
if [ -d /dev/stune/rt ]; then
    echo 1 > /dev/stune/rt/schedtune.prefer_idle 2>/dev/null || true
    echo 1 > /dev/stune/rt/schedtune.boost 2>/dev/null || true
    echo 1 > /dev/stune/rt/schedtune.sched_boost_no_override 2>/dev/null || true
fi
if [ -d /dev/stune/foreground ]; then
    echo 1 > /dev/stune/foreground/schedtune.prefer_idle 2>/dev/null || true
    echo 0 > /dev/stune/foreground/schedtune.boost 2>/dev/null || true
fi
if [ -d /dev/stune/background ]; then
    echo 0 > /dev/stune/background/schedtune.prefer_idle 2>/dev/null || true
    echo 0 > /dev/stune/background/schedtune.boost 2>/dev/null || true
fi
for gpu_gov in /sys/class/kgsl/kgsl-3d0/devfreq/governor; do
    [ -f "$gpu_gov" ] && echo "msm-adreno-tz" > "$gpu_gov" 2>/dev/null || true
done
for gpu_idle in /sys/class/kgsl/kgsl-3d0/idle_timer; do
    [ -f "$gpu_idle" ] && echo 50 > "$gpu_idle" 2>/dev/null || true
done

# 7. AMOLED DC Dimming, Standard Touch Sensitivity & Haptics
for d in /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker; do
    [ -f "$d" ] && echo 1 > "$d" 2>/dev/null || true
done
for g in /sys/devices/platform/soc/*.i2c/i2c-*/1-*/fts_glove_mode /sys/bus/i2c/drivers/fts_ts/*/fts_glove_mode; do
    [ -f "$g" ] && echo 0 > "$g" 2>/dev/null || true
done
echo 1 > /sys/class/leds/vibrator/haptic_mode 2>/dev/null || true
echo 15 > /sys/class/leds/vibrator/click_duration 2>/dev/null || true

# 8. Battery Charging, Fastcharge & Thermal permissions (Open permissions for user monitoring, zero forced overrides)
for chg_node in \
    /sys/class/power_supply/battery/charging_enabled \
    /sys/class/power_supply/battery/battery_charging_enabled \
    /sys/class/power_supply/battery/constant_charge_current_max \
    /sys/class/power_supply/battery/step_charging_enabled \
    /sys/class/power_supply/battery/fastcharge_mode \
    /sys/class/power_supply/bms/fastcharge_mode
do
    [ -f "$chg_node" ] && chmod 0666 "$chg_node" && chown system:system "$chg_node" 2>/dev/null || true
done

# 9. AETHER Hardware Charge Cutoff & Bypass Persistence (Offline & Normal Boot)
if [ -f /mnt/vendor/persist/aether/cutoff.conf ]; then
    . /mnt/vendor/persist/aether/cutoff.conf 2>/dev/null
    [ -n "$CUTOFF_LIMIT" ] && echo "$CUTOFF_LIMIT" > /sys/kernel/aether/charge_cutoff_limit 2>/dev/null || true
    [ -n "$CUTOFF_MODE" ] && echo "$CUTOFF_MODE" > /sys/kernel/aether/charge_cutoff_mode 2>/dev/null || true
fi

# 10. Asynchronous Boot Completion Optimizer (Two-Stage Scaling & Memory Conservation)
(
    # Poll sys.boot_completed until boot finishes (max 90 seconds timeout)
    boot_ticks=0
    while [ "$(getprop sys.boot_completed)" != "1" ] && [ $boot_ticks -lt 90 ]; do
        sleep 1
        boot_ticks=$((boot_ticks + 1))
    done

    # Allow 15 seconds after first unlock for launcher & initial widgets to settle smoothly
    sleep 15

    # Stage 2: Scale down UFS read-ahead buffer to 256KB to conserve RAM and prevent cache bloat
    for q in /sys/block/sd*/queue /sys/block/dm-*/queue; do
        if [ -d "$q" ]; then
            echo 256 > "$q/read_ahead_kb" 2>/dev/null || true
            echo 128 > "$q/nr_requests" 2>/dev/null || true
        fi
    done

    # Re-enforce background cpuset isolation to keep idle drain at zero
    if [ -d /dev/cpuset/background ]; then
        echo "0-3" > /dev/cpuset/background/cpus 2>/dev/null || true
    fi
    if [ -d /dev/cpuset/system-background ]; then
        echo "0-3" > /dev/cpuset/system-background/cpus 2>/dev/null || true
    fi

    # Silent background TRIM once to optimize UFS flash write wear
    fstrim -v /data 2>/dev/null || true
) &
EOF
    chmod 0755 /data/adb/service.d/01_displayfeatures.sh;
    chown 0:0 /data/adb/service.d/01_displayfeatures.sh;
    ui_print "        └─ Display, storage, network, audio & scheduler services installed";

    # Ensure open permissions on custom tuneable nodes
    chmod 0666 /sys/kernel/aether/* 2>/dev/null || true;
    chmod 0666 /proc/sys/kernel/sched_bore* 2>/dev/null || true;
    chmod 0666 /sys/module/cpu_input_boost/parameters/* 2>/dev/null || true;
    chmod 0666 /sys/module/devfreq_boost/parameters/* 2>/dev/null || true;
    chmod 0666 /sys/devices/system/cpu/cpufreq/policy*/schedutil/* 2>/dev/null || true;
    chmod 0666 /proc/sys/net/ipv4/tcp_congestion_control 2>/dev/null || true;
    chmod 0666 /proc/sys/net/core/default_qdisc 2>/dev/null || true;
    chmod 0666 /proc/sys/net/ipv4/tcp_fastopen 2>/dev/null || true;
    chmod 0666 /proc/sys/vm/vfs_cache_pressure 2>/dev/null || true;
    chmod 0666 /sys/block/sd*/queue/* 2>/dev/null || true;
    chmod 0666 /sys/block/zram*/queue/* 2>/dev/null || true;
    chmod 0666 /sys/fs/f2fs/*/* 2>/dev/null || true;
    chmod 0666 /proc/sys/kernel/sched_energy_aware 2>/dev/null || true;
    chmod 0666 /sys/module/workqueue/parameters/power_efficient 2>/dev/null || true;
    chmod 0666 /sys/class/kgsl/kgsl-3d0/* 2>/dev/null || true;
    chmod 0666 /sys/class/kgsl/kgsl-3d0/devfreq/* 2>/dev/null || true;
    chmod 0666 /sys/kernel/mm/lru_gen/* 2>/dev/null || true;
    chmod 0666 /sys/class/power_supply/battery/* 2>/dev/null || true;
    chmod 0666 /sys/class/power_supply/bms/* 2>/dev/null || true;
    chmod 0666 /sys/class/leds/vibrator/* 2>/dev/null || true;
    chmod 0666 /sys/devices/platform/soc/*.qcom,vibrator/linear_vib* 2>/dev/null || true;
    chmod 0666 /sys/devices/platform/soc/soc:qcom,dsi-display/* 2>/dev/null || true;
    chmod 0666 /sys/devices/platform/soc/*.i2c/i2c-*/1-*/fts_* 2>/dev/null || true;
    chmod 0666 /sys/bus/i2c/drivers/fts_ts/*/fts_* 2>/dev/null || true;
    chmod 0666 /sys/module/tx_macro_dlkm/parameters/* 2>/dev/null || true;
    chmod 0666 /sys/module/tx_macro/parameters/* 2>/dev/null || true;
    chmod 0666 /dev/snd/* 2>/dev/null || true;
fi;

sleep 0.2;
ui_print "        [██████████] 100%  Configuration complete";
ui_print " ";

ui_print "  ╔══════════════════════════════════════════════╗";
ui_print "  ║            Installation Complete             ║";
ui_print "  ║        Reboot system to apply changes        ║";
ui_print "  ╚══════════════════════════════════════════════╝";
ui_print " ";
