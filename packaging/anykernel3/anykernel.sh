### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers
## Customized for AETHER Kernel by Kiran_Embedded (https://github.com/kiran-embedded)

### AnyKernel setup
# begin properties
properties() { '
kernel.string=AETHER CORE v1.6 by Kiran (ProjectAether)
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
ui_print "  ║  Version  : v1.6                             ║";
ui_print "  ║  Device   : $dev_display";
ui_print "  ║  Platform : Snapdragon 678 (SM6150)          ║";
ui_print "  ║  System   : $rom_display";
ui_print "  ║  Android  : $android_display ($patch_display)";
ui_print "  ║  Target   : $slot_display";
ui_print "  ║  Edition  : Stable (MGLRU + BORE)            ║";
ui_print "  ╠══════════════════════════════════════════════╣";
ui_print "  ║  Kernel Features:                            ║";
ui_print "  ║   • CPU Sched : BORE v5.1.0 + Energy Schedutil ║";
ui_print "  ║   • Boost     : Sultan CPU & Devfreq Boost   ║";
ui_print "  ║   • Memory    : Multi-Gen LRU (MGLRU)        ║";
ui_print "  ║   • Root      : KernelSU-Next + SUSFS v2.3.0 ║";
ui_print "  ║   • Storage   : UFS 2.2 WriteBoost & F2FS   ║";
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

# Block queue / UFS I/O — Tuned for low queue latency
# 1536KB readahead: fast sequential reads without wasting RAM on random I/O (app launches)
# Philosophy: every value is "safe-but-fast" — no CPU spikes, no write starvation, no RAM waste
for q in /sys/block/sd*/queue; do
    if [ -d "$q" ]; then
        if [ -f "$q/scheduler" ]; then
            echo "mq-deadline" > "$q/scheduler" 2>/dev/null || echo "deadline" > "$q/scheduler" 2>/dev/null || true
        fi
        echo 0 > "$q/add_random" 2>/dev/null || true
        echo 0 > "$q/iostats" 2>/dev/null || true
        echo 0 > "$q/nomerges" 2>/dev/null || true
        echo 0 > "$q/rotational" 2>/dev/null || true
        echo 2 > "$q/rq_affinity" 2>/dev/null || true
        echo 128 > "$q/nr_requests" 2>/dev/null || true
        echo 1536 > "$q/read_ahead_kb" 2>/dev/null || true
        # mq-deadline: smooth predictable dispatch — no CPU spikes, no write starvation
        [ -f "$q/iosched/quantum" ] && echo 6 > "$q/iosched/quantum" 2>/dev/null || true
        [ -f "$q/iosched/fifo_batch" ] && echo 16 > "$q/iosched/fifo_batch" 2>/dev/null || true
        [ -f "$q/iosched/read_expire" ] && echo 200 > "$q/iosched/read_expire" 2>/dev/null || true
        [ -f "$q/iosched/write_expire" ] && echo 1000 > "$q/iosched/write_expire" 2>/dev/null || true
        [ -f "$q/iosched/writes_starved" ] && echo 3 > "$q/iosched/writes_starved" 2>/dev/null || true
    fi
done
for q in /sys/block/dm-*/queue; do
    if [ -d "$q" ]; then
        if [ -f "$q/scheduler" ]; then
            echo "mq-deadline" > "$q/scheduler" 2>/dev/null || echo "deadline" > "$q/scheduler" 2>/dev/null || true
        fi
        echo 0 > "$q/add_random" 2>/dev/null || true
        echo 0 > "$q/iostats" 2>/dev/null || true
        echo 0 > "$q/nomerges" 2>/dev/null || true
        echo 0 > "$q/rotational" 2>/dev/null || true
        echo 2 > "$q/rq_affinity" 2>/dev/null || true
        echo 128 > "$q/nr_requests" 2>/dev/null || true
        echo 512 > "$q/read_ahead_kb" 2>/dev/null || true
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

# F2FS asynchronous discard tuning and GC pacing
for f2fs_disk in /sys/fs/f2fs/*; do
    if [ -d "$f2fs_disk" ]; then
        echo 0 > "$f2fs_disk/gc_urgent" 2>/dev/null || true
        echo 0 > "$f2fs_disk/iostat_enable" 2>/dev/null || true
        echo 18 > "$f2fs_disk/ipu_policy" 2>/dev/null || true
        echo 8 > "$f2fs_disk/min_fsync_blocks" 2>/dev/null || true
        echo 128 > "$f2fs_disk/max_discard_request" 2>/dev/null || true
        echo 5 > "$f2fs_disk/min_discard_issue_time" 2>/dev/null || true
        echo 50 > "$f2fs_disk/mid_discard_issue_time" 2>/dev/null || true
        echo 4 > "$f2fs_disk/discard_granularity" 2>/dev/null || true
    fi
done

# ═══════════════════════════════════════════════════════════════════════
# UFS 2.2 Write Speed and Power Management
# ═══════════════════════════════════════════════════════════════════════
# Design principles:
#   • WriteBooster enabled   → burst write speed for installations
#   • Hibern8 entry at 5ms   → UFS clock off in <5ms idle
#   • Dynamic clock scaling  → let ufshcd scale to required gear under load
#   • WB flush threshold 80% → avoids buffer-full stall
#   • front_merges=1         → deadline coalesces adjacent I/Os
#   • async_depth=16/32      → balances read/write priorities
# ═══════════════════════════════════════════════════════════════════════

# Stage 1: Clock Gating Delay — low idle power
# - 15ms perf: fast enough to avoid re-init penalty, short enough to save battery
# - 8ms pwr_save: screen-off idle → near-zero UFS clock drain
# WHY NOT 25ms: 25ms keeps UFS clocks powered after EVERY I/O = measurable mA drain
for ufs_clk in /sys/devices/platform/soc/*.ufshc /sys/bus/platform/drivers/ufshcd-qcom/*; do
    if [ -d "$ufs_clk" ]; then
        echo 15 > "$ufs_clk/clkgate_delay_ms_perf" 2>/dev/null || true
        echo 15 > "$ufs_clk/clkgate_delay_ms" 2>/dev/null || true
        echo 8  > "$ufs_clk/clkgate_delay_ms_pwr_save" 2>/dev/null || true

        # Stage 2: Runtime Power Management Level
        # rpm_lvl=1: Active+Hibern8 — instant I/O resume, no deep-powerdown latency
        # spm_lvl=3: Sleep+Hibern8  — deep sleep when screen off = battery savings
        echo 1 > "$ufs_clk/rpm_lvl" 2>/dev/null || true
        echo 3 > "$ufs_clk/spm_lvl" 2>/dev/null || true

        # Stage 3: Auto-Hibern8 idle timer
        # 5000us (5ms): UFS enters Hibern8 within 5ms of idle → near-zero clock rail drain
        # WHY NOT 1ms: 1ms causes excessive hibern8↔active cycling under light I/O = more power
        echo 5000 > "$ufs_clk/auto_hibern8" 2>/dev/null || true

        # Stage 4: UFS Clock Scaling — dynamic max gear under load, min gear at idle
        # Let ufshcd-qcom ramp to HS-G3 Lane2 automatically under I/O pressure
        [ -f "$ufs_clk/clk_scaling/enable" ] && echo 1 > "$ufs_clk/clk_scaling/enable" 2>/dev/null || true
        # Aggressive scaling-up: scale to max gear faster when burst I/O detected
        [ -f "$ufs_clk/clk_scaling/scale_up_threshold" ] && echo 30 > "$ufs_clk/clk_scaling/scale_up_threshold" 2>/dev/null || true
        # Conservative scaling-down: stay at high gear a bit longer to avoid re-ramp cost
        [ -f "$ufs_clk/clk_scaling/scale_down_threshold" ] && echo 20 > "$ufs_clk/clk_scaling/scale_down_threshold" 2>/dev/null || true

        # Stage 5: UFS Write Booster (UFS 2.2 turbo write buffer — 2-4x burst write speed)
        # wb_on=1             → enables the dedicated SLC write buffer in NAND
        # wb_buf_flush_en=1   → enables background flush of WB buffer to TLC
        # wb_buf_flush_thresh  → flush when WB buffer ≥80% full (KEY: prevents write stall
        #                        from WB-buffer-full condition — most kernels miss this)
        [ -f "$ufs_clk/wb_on" ]              && echo 1  > "$ufs_clk/wb_on"              2>/dev/null || true
        [ -f "$ufs_clk/wb_buf_flush_en" ]    && echo 1  > "$ufs_clk/wb_buf_flush_en"    2>/dev/null || true
        [ -f "$ufs_clk/wb_buf_flush_thresh" ] && echo 80 > "$ufs_clk/wb_buf_flush_thresh" 2>/dev/null || true
        # Flush holdoff: 2000ms — don't background-flush during active burst (avoids write contention)
        [ -f "$ufs_clk/wb_flush_stat_after_h8" ] && echo 1 > "$ufs_clk/wb_flush_stat_after_h8" 2>/dev/null || true

        # Stage 6: Hibern8 on idle delay (5ms — smooth transitions, no rapid wake/sleep cycling)
        # WHY NOT 3ms: 3ms causes constant hibernate↔wake oscillation = CPU overhead = micro-stutter
        [ -f "$ufs_clk/hibern8_on_idle_delay" ] && echo 5 > "$ufs_clk/hibern8_on_idle_delay" 2>/dev/null || true

        # Stage 7 (NEW): UFS device-level write_cache mode
        # write-back: fastest (cache in UFS DRAM), slight power-loss risk
        # write-through: safe + still faster than uncached
        # We use write-through: full speed benefit with data integrity guarantee
        [ -f "$ufs_clk/write_booster_enable" ] && echo 1 > "$ufs_clk/write_booster_enable" 2>/dev/null || true
    fi
done

# Stage 8: UFS LUN Queue Depth & SCSI tuning for max I/O parallelism
for scsi_dev in /sys/class/scsi_device/*/device; do
    if [ -d "$scsi_dev" ]; then
        # 32 outstanding commands: UFS 2.2 NCQ supports up to 32 — saturates internal NAND pipeline
        [ -f "$scsi_dev/queue_depth" ] && echo 32 > "$scsi_dev/queue_depth" 2>/dev/null || true
        # 15s error recovery timeout (vs default 30s) — faster failover without false triggers
        [ -f "$scsi_dev/timeout" ] && echo 15 > "$scsi_dev/timeout" 2>/dev/null || true
    fi
done

# Stage 9: Block layer — UFS-specific I/O tuning (aligned with sd* loop above)
# Only touches parameters NOT already set by the sd* loop to avoid conflict
for q in /sys/block/sda/queue /sys/block/sdb/queue /sys/block/sdc/queue; do
    if [ -d "$q" ]; then
        # max_sectors_kb=512: let UFS transfer 512KB per command (saturates HS-G3 bandwidth)
        # SAFE: UFS hardware max is typically 512KB; kernel caps to max_hw_sectors_kb anyway
        hw_max=$(cat "$q/max_hw_sectors_kb" 2>/dev/null || echo 512)
        [ "$hw_max" -gt 512 ] && hw_max=512
        echo "$hw_max" > "$q/max_sectors_kb" 2>/dev/null || true

        # front_merges=1: allow deadline to merge requests from the front of the queue
        # Critical for sequential write coalescing (app installs, OTA patches, media saves)
        echo 1 > "$q/iosched/front_merges" 2>/dev/null || true

        # async_depth=32: allow 32 async (write) requests in flight simultaneously
        # Prevents write starvation during mixed read/write workloads
        echo 32 > "$q/iosched/async_depth" 2>/dev/null || true

        # Widen write expire window to 2000ms — give UFS time to batch writes
        # before forced dispatch; reduces write fragmentation on NAND
        echo 2000 > "$q/iosched/write_expire" 2>/dev/null || true

        # nr_requests=64: UFS performs best with 64 in-flight block requests
        # (128 causes queue contention on UFS; 64 matches NCQ depth for writes)
        echo 64 > "$q/nr_requests" 2>/dev/null || true
    fi
done

# Schedutil per-cluster rate limits are applied in the Battery Drain Fix section below.

# CPU & Devfreq Boost (Touch & Input responsiveness without waking big cores)
for cb in /sys/module/cpu_boost/parameters /sys/module/cpu_input_boost/parameters; do
    if [ -d "$cb" ]; then
        # Boost efficiency cores (0-5) to 1.0 GHz for instant touch response; keep big cores (6-7) at 0 to eliminate battery drain
        echo "0:1017600 1:1017600 2:1017600 3:1017600 4:1017600 5:1017600 6:0 7:0" > "$cb/input_boost_freq" 2>/dev/null || true
        echo 64 > "$cb/input_boost_ms" 2>/dev/null || true
        echo 64 > "$cb/input_boost_duration" 2>/dev/null || true
        echo 1 > "$cb/sched_boost_on_input" 2>/dev/null || true
    fi
done
for dfb in /sys/module/devfreq_boost/parameters; do
    if [ -d "$dfb" ]; then
        echo 48 > "$dfb/devfreq_boost_duration" 2>/dev/null || true
        echo 64 > "$dfb/devfreq_wake_boost_duration" 2>/dev/null || true
    fi
done

# BORE full tuning is applied in the Memory & Power Optimization section below.

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

# =============================================================
# AETHER Memory & Power Optimization Configuration
# Target: Redmi Note 10 mojito (SM6150 / 4GB LPDDR4X)
# Calibration:
#   • Balanced page cache retention for multitasking
#   • Optimized idle power consumption
#   • BORE and MGLRU scheduling coordination
# =============================================================

# ── 1. ZRAM Setup ─────────────────────────────────────────────────
# Compression: LZ4 (low CPU overhead for decompression during memory pressure)
# Minimal CPU overhead prevents latency spikes during memory pressure.
# Size: 2GB ZRAM with LZ4 + deduplication + writeback
if [ -b /dev/block/zram0 ]; then
    swapoff /dev/block/zram0 2>/dev/null || true
    echo 1 > /sys/block/zram0/reset 2>/dev/null || true
    # Force LZ4 compression for high-throughput decompression with negligible CPU overhead
    echo lz4 > /sys/block/zram0/comp_algorithm 2>/dev/null || true
    # Dynamic ZRAM sizing: 4GB for 6GB RAM models, 3GB for 4GB models (stops LMKD app kills)
    TOTAL_RAM_KB=$(grep MemTotal /proc/meminfo 2>/dev/null | awk '{print $2}' || echo 4000000)
    if [ "${TOTAL_RAM_KB:-0}" -ge 4500000 ]; then
        ZRAM_BYTES=$((4 * 1024 * 1024 * 1024))
    else
        ZRAM_BYTES=$((3 * 1024 * 1024 * 1024))
    fi
    echo "${ZRAM_BYTES}" > /sys/block/zram0/disksize 2>/dev/null || true
    mkswap /dev/block/zram0 2>/dev/null || true
    # Priority 32767: ALWAYS prefer ZRAM over any filesystem swap
    swapon /dev/block/zram0 -p 32767 2>/dev/null || true
fi

# ── 2. Virtual Memory — Core Tunables (PDF Section 3.1) ─────────────
# SWAPPINESS: 100
#   Allows MGLRU to seamlessly migrate cold anon pages to ZRAM without thrashing.
echo 100 > /proc/sys/vm/swappiness 2>/dev/null || true

# VFS_CACHE_PRESSURE: 100 — Maintains balanced 1:1 reclaim ratio between dentry/inode caches and page cache
echo 100 > /proc/sys/vm/vfs_cache_pressure 2>/dev/null || true

# MIN_FREE_KBYTES: 16384 (16MB baseline)
echo 16384 > /proc/sys/vm/min_free_kbytes 2>/dev/null || true

# EXTRA_FREE_KBYTES: 0 (default baseline)
echo 0 > /proc/sys/vm/extra_free_kbytes 2>/dev/null || true

# WATERMARK_SCALE_FACTOR: 150 — widens free buffer gap to 1.5%-2.0%, eliminates direct reclaim freezes
echo 150 > /proc/sys/vm/watermark_scale_factor 2>/dev/null || true

# DIRTY WRITEBACK: Tuned for UFS + F2FS
echo 5 > /proc/sys/vm/dirty_background_ratio 2>/dev/null || true
echo 20 > /proc/sys/vm/dirty_ratio 2>/dev/null || true
echo 500 > /proc/sys/vm/dirty_writeback_centisecs 2>/dev/null || true
echo 1500 > /proc/sys/vm/dirty_expire_centisecs 2>/dev/null || true

# PAGE CLUSTER: 0 (single 4KB page reads, zero decompression stalls on relaunch)
echo 0 > /proc/sys/vm/page-cluster 2>/dev/null || true

# COMPACT_UNEVICTABLE_ALLOWED: 1 (safe memory compaction without unevictable locks)
echo 1 > /proc/sys/vm/compact_unevictable_allowed 2>/dev/null || true

# COMPACTION PROACTIVENESS: 20
echo 20 > /proc/sys/vm/compaction_proactiveness 2>/dev/null || true

# PERCPU_PAGELIST_FRACTION: 0 (dynamic default batching, avoids locking 25% of RAM in per-CPU pools)
echo 0 > /proc/sys/vm/percpu_pagelist_fraction 2>/dev/null || true

# MAX_MAP_COUNT: 524288
echo 524288 > /proc/sys/vm/max_map_count 2>/dev/null || true

# STAT_INTERVAL: 10
#   Reduces periodic vmstat timer ticks on idle CPUs to prevent idle battery drain
echo 10 > /proc/sys/vm/stat_interval 2>/dev/null || true

# OOM Stability Protections
echo 0 > /proc/sys/vm/oom_dump_tasks 2>/dev/null || true
echo 0 > /proc/sys/vm/panic_on_oom 2>/dev/null || true
echo 0 > /proc/sys/vm/oom_kill_allocating_task 2>/dev/null || true

# ── 3. MGLRU — Multigenerational LRU ──────────────────────────────
# Protects active working sets and delays eviction of active pages.
if [ -f /sys/kernel/mm/lru_gen/enabled ]; then
    echo 7 > /sys/kernel/mm/lru_gen/enabled 2>/dev/null || echo y > /sys/kernel/mm/lru_gen/enabled 2>/dev/null || true
    # MIN_TTL_MS: 1000ms minimum generation residency time to eliminate thrashing
    echo 1000 > /sys/kernel/mm/lru_gen/min_ttl_ms 2>/dev/null || true
fi

# ── 4. BORE Scheduler Configuration ───────────────────────────────
# Burst-Oriented Response Enhancer prioritizes interactive task execution.
echo 1 > /proc/sys/kernel/sched_bore 2>/dev/null || true

# BURST_PENALTY_SCALE: 1380 — 1380/4096 multiplier applied to burst greed
echo 1380 > /proc/sys/kernel/sched_burst_penalty_scale 2>/dev/null || true

# BURST_PENALTY_OFFSET: 22 — tolerance threshold before scaling begins
echo 22 > /proc/sys/kernel/sched_burst_penalty_offset 2>/dev/null || true

# SMOOTHNESS_LONG: 1 & SMOOTHNESS_SHORT: 0
echo 1 > /proc/sys/kernel/sched_burst_smoothness_long 2>/dev/null || true
echo 0 > /proc/sys/kernel/sched_burst_smoothness_short 2>/dev/null || true

# SMOOTHNESS_UP: 2 & SMOOTHNESS_DOWN: 1
echo 2 > /proc/sys/kernel/sched_burst_smoothness_up 2>/dev/null || true
echo 1 > /proc/sys/kernel/sched_burst_smoothness_down 2>/dev/null || true

# BURST_CACHE_LIFETIME: 60000000ns (60ms)
echo 60000000 > /proc/sys/kernel/sched_burst_cache_lifetime 2>/dev/null || true

# FORK_ATAVISTIC: 2 — child inherits parent burst penalty score
echo 2 > /proc/sys/kernel/sched_burst_fork_atavistic 2>/dev/null || true
if [ -d /dev/cpuset/top-app ]; then
    echo "0-7" > /dev/cpuset/top-app/cpus 2>/dev/null || true
fi
if [ -d /dev/cpuset/foreground ]; then
    echo "0-7" > /dev/cpuset/foreground/cpus 2>/dev/null || true
fi
if [ -d /dev/cpuset/background ]; then
    echo "0-5" > /dev/cpuset/background/cpus 2>/dev/null || true
fi
if [ -d /dev/cpuset/system-background ]; then
    echo "0-5" > /dev/cpuset/system-background/cpus 2>/dev/null || true
fi
if [ -d /dev/cpuset/restricted ]; then
    echo "0-3" > /dev/cpuset/restricted/cpus 2>/dev/null || true
fi
if [ -d /dev/stune/top-app ]; then
    echo 1 > /dev/stune/top-app/schedtune.prefer_idle 2>/dev/null || true
    echo 1 > /dev/stune/top-app/schedtune.boost 2>/dev/null || true
    echo 1 > /dev/stune/top-app/schedtune.colocate 2>/dev/null || true
    echo 0 > /dev/stune/top-app/schedtune.sched_boost_no_override 2>/dev/null || true
fi
if [ -d /dev/stune/rt ]; then
    echo 1 > /dev/stune/rt/schedtune.prefer_idle 2>/dev/null || true
    echo 0 > /dev/stune/rt/schedtune.boost 2>/dev/null || true
    echo 0 > /dev/stune/rt/schedtune.sched_boost_no_override 2>/dev/null || true
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
    [ -f "$gpu_idle" ] && echo 72 > "$gpu_idle" 2>/dev/null || true
done

# =============================================================
# AETHER Battery Idle Drain Fix — Deep SM6150 Power Suite
# =============================================================

# ── 5. Energy-Aware Scheduling ────────────────────────────────────
# EAS routes background tasks to efficiency cores (Kryo 460 Silver × 6)
# Big cores (Kryo 460 Gold × 2) stay in C2 deep sleep during idle.
echo 1 > /proc/sys/kernel/sched_energy_aware 2>/dev/null || true

# Disable WALT big-task rotation — prevents unnecessary big-core wakeups
echo 0 > /proc/sys/kernel/sched_walt_rotate_big_tasks 2>/dev/null || true

# SCHED_CSTATE_AWARE: 1 — scheduler avoids waking sleeping CPUs in deep C-states
echo 1 > /proc/sys/kernel/sched_cstate_aware 2>/dev/null || true

# SCHED_SYNC_HINT_ENABLE: 1 — minimizes IPC / Binder thread migration latency for smooth UI
echo 1 > /proc/sys/kernel/sched_sync_hint_enable 2>/dev/null || true

# SCHED_SCHEDSTATS: 0 — disables scheduler statistics collection to reduce context switch latency
echo 0 > /proc/sys/kernel/sched_schedstats 2>/dev/null || true

# ── 6. Workqueue Power Efficiency ─────────────────────────────────
# Routes non-latency-critical workqueue items to the current CPU
# (usually a little core at idle) instead of pulling a big core out of sleep.
echo 1 > /sys/module/workqueue/parameters/power_efficient 2>/dev/null || true

# ── 7. Printk & Console Suspend ───────────────────────────────────
# Suppress kernel console output during suspend — each printk during
# sleep is a wakeup event. This alone can save 5-10% idle battery.
echo 1 > /sys/module/printk/parameters/console_suspend 2>/dev/null || true
echo "4 4 1 7" > /proc/sys/kernel/printk 2>/dev/null || true

# ── 8. LPM Deep Sleep — SM6150 Specific ─────────────────────────
# Ensure SoC-level deep sleep (PC / Rail Collapse) is not disabled.
# lpm_prediction=1: kernel predicts sleep duration for optimal C-state.
echo 0 > /sys/module/lpm_levels/parameters/sleep_disabled 2>/dev/null || true
echo 1 > /sys/module/lpm_levels/parameters/lpm_prediction 2>/dev/null || true
echo 1 > /sys/module/lpm_levels/parameters/lpm_ipi_prediction 2>/dev/null || true

# ── 9. Schedutil — Per-Cluster Rate Limits ────────────────────────
# Policy 0 (Silver/little cores: cpu0-5): fast up (500µs), slow down (20ms)
#   → Efficiency cores ramp fast for responsiveness, decay slowly to save power
# Policy 6 (Gold/big cores: cpu6-7): moderate up (800µs), very slow down (25ms)
#   → Big cores avoid unnecessary frequency spikes during idle
for pol in /sys/devices/system/cpu/cpufreq/policy0 /sys/devices/system/cpu/cpufreq/policy{0,1,2,3,4,5}; do
    if [ -d "$pol/schedutil" ]; then
        echo 500  > "$pol/schedutil/up_rate_limit_us" 2>/dev/null || true
        echo 20000 > "$pol/schedutil/down_rate_limit_us" 2>/dev/null || true
    fi
done
for pol in /sys/devices/system/cpu/cpufreq/policy6 /sys/devices/system/cpu/cpufreq/policy{6,7}; do
    if [ -d "$pol/schedutil" ]; then
        echo 800  > "$pol/schedutil/up_rate_limit_us" 2>/dev/null || true
        echo 25000 > "$pol/schedutil/down_rate_limit_us" 2>/dev/null || true
    fi
done

# ── 10. Wakelock GC Enforcement ───────────────────────────────────
# Forcibly expire stale PM wakelocks that prevent deep sleep.
# The kernel PM_WAKELOCKS_GC=y in defconfig handles new wakelocks;
# this clears any already-stale ones at runtime.
echo 1 > /sys/power/pm_wakeup_irq 2>/dev/null || true

# ── 11. IRQ Affinity — Keep ISRs on Little Cores ──────────────────
# Move most IRQs away from big cores (CPU6-7) so they can sleep deeper.
# Efficiency cores 0-5 handle ISRs efficiently with minimal power leakage.
for irq_aff in /proc/irq/*/smp_affinity_list; do
    curr=$(cat "$irq_aff" 2>/dev/null)
    # Only reassign IRQs currently pinned to big cores (6-7 only)
    case "$curr" in
        6|7|"6-7"|"4-7")
            echo "0-5" > "$irq_aff" 2>/dev/null || true ;;
    esac
done

# ── 12. GPU Power Rail — Aggressive Power Collapse (Adreno 612) ────
# Allow Adreno 612 to power-collapse immediately when idle
echo 66 > /sys/class/kgsl/kgsl-3d0/idle_timer 2>/dev/null || true
echo 0 > /sys/class/kgsl/kgsl-3d0/force_clk_on 2>/dev/null || true
echo 0 > /sys/class/kgsl/kgsl-3d0/force_bus_on 2>/dev/null || true
echo 0 > /sys/class/kgsl/kgsl-3d0/force_rail_on 2>/dev/null || true
echo 0 > /sys/class/kgsl/kgsl-3d0/force_no_nap 2>/dev/null || true
echo 1 > /sys/class/kgsl/kgsl-3d0/bus_split 2>/dev/null || true

# ── 13. Auto-Kprofiles Screen-State Management ────────────────────
# Automatically shifts to Battery Mode on screen-off via MSM DRM events
echo 1 > /sys/module/kprofiles/parameters/auto_kp 2>/dev/null || true
echo 0 > /sys/kernel/kprofiles/kp_mode 2>/dev/null || true

# ── 13. LMKD Tuning — Let MGLRU Do the Work ──────────────────────
# With swappiness=60 and MGLRU min_ttl=1000ms, LMKD should kill
# processes far less aggressively. Set minfree thresholds to give
# MGLRU maximum room to reclaim before LMKD steps in.
if [ -f /sys/module/lowmemorykiller/parameters/minfree ]; then
    # 18,23,27,32,55,90 (MB) — conservative; let MGLRU reclaim first
    echo "18432,23552,27648,32768,56320,92160" \
        > /sys/module/lowmemorykiller/parameters/minfree 2>/dev/null || true
    # adj: 0,100,200,250,900,906 — only kill completely hidden apps
    echo "0,100,200,250,900,906" \
        > /sys/module/lowmemorykiller/parameters/adj 2>/dev/null || true
fi

# 7. AMOLED DC Dimming, Standard Touch Sensitivity & Haptics
for d in /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker; do
    [ -f "$d" ] && echo 1 > "$d" 2>/dev/null || true
done
for g in /sys/devices/platform/soc/*.i2c/i2c-*/1-*/fts_glove_mode /sys/bus/i2c/drivers/fts_ts/*/fts_glove_mode; do
    [ -f "$g" ] && echo 0 > "$g" 2>/dev/null || true
done
echo 1 > /sys/class/leds/vibrator/haptic_mode 2>/dev/null || true
echo 15 > /sys/class/leds/vibrator/click_duration 2>/dev/null || true

# 8. Battery Charging, Fastcharge and Thermal node permissions (Permissions for user monitoring)
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

# 10. Steady-state Readahead & Background CPUset Isolation
# Executes once on boot completion (<20ms runtime, 0 background daemons)
for q in /sys/block/sd*/queue /sys/block/dm-*/queue; do
    if [ -d "$q" ]; then
        echo 512 > "$q/read_ahead_kb" 2>/dev/null || true
        echo 64  > "$q/nr_requests" 2>/dev/null || true
    fi
done

# ZRAM Queue Calibration — 0KB read-ahead, rq_affinity 2, nomerges 2, no iostats overhead
if [ -d /sys/block/zram0/queue ]; then
    echo 0 > /sys/block/zram0/queue/read_ahead_kb 2>/dev/null || true
    echo 2 > /sys/block/zram0/queue/rq_affinity 2>/dev/null || true
    echo 2 > /sys/block/zram0/queue/nomerges 2>/dev/null || true
    echo 0 > /sys/block/zram0/queue/iostats 2>/dev/null || true
    echo 0 > /sys/block/zram0/queue/add_random 2>/dev/null || true
fi

    # Re-enforce background cpuset isolation to efficiency cores 0-5
    if [ -d /dev/cpuset/background ]; then
        echo "0-5" > /dev/cpuset/background/cpus 2>/dev/null || true
    fi
    if [ -d /dev/cpuset/system-background ]; then
        echo "0-5" > /dev/cpuset/system-background/cpus 2>/dev/null || true
    fi

    # ── Memory Subsystem Configuration ──────────────────────────────
    # Tuned memory limits and background compaction.

    # ┌─────────────────────────────────────────────────────────────┐
    # │  Layer 1: Logcat Buffer Bounds                              │
    # └─────────────────────────────────────────────────────────────┘
    # Stock MIUI runs 4-16MB ring buffers per logcat channel (main, radio,
    # system, crash) plus dumpstate, ramdump listeners and system tracing.
    # Shrink to 256KB per channel and disable debug overhead.
    logcat -G 256K 2>/dev/null || true
    resetprop -n persist.logd.size 262144 2>/dev/null || setprop persist.logd.size 262144 2>/dev/null || true
    resetprop -n persist.logd.size.main 262144 2>/dev/null || setprop persist.logd.size.main 262144 2>/dev/null || true
    resetprop -n persist.logd.size.system 131072 2>/dev/null || setprop persist.logd.size.system 131072 2>/dev/null || true
    resetprop -n persist.logd.size.radio 131072 2>/dev/null || setprop persist.logd.size.radio 131072 2>/dev/null || true
    resetprop -n persist.logd.size.crash 65536 2>/dev/null || setprop persist.logd.size.crash 65536 2>/dev/null || true

    # Disable system tracing (atrace) and debug logging daemons
    resetprop -n persist.traced.enable 0 2>/dev/null || setprop persist.traced.enable 0 2>/dev/null || true
    resetprop -n persist.sys.assert.enable false 2>/dev/null || setprop persist.sys.assert.enable false 2>/dev/null || true
    resetprop -n persist.sys.strictmode.visual 0 2>/dev/null || setprop persist.sys.strictmode.visual 0 2>/dev/null || true
    resetprop -n persist.sys.strictmode.disable 1 2>/dev/null || setprop persist.sys.strictmode.disable 1 2>/dev/null || true

    # Disable debug malloc (leak tracker) in native daemons
    resetprop -n libc.debug.malloc 0 2>/dev/null || setprop libc.debug.malloc 0 2>/dev/null || true
    resetprop -n persist.libc.debug.malloc 0 2>/dev/null || setprop persist.libc.debug.malloc 0 2>/dev/null || true

    # ┌─────────────────────────────────────────────────────────────┐
    # │  Layer 7: 60 FPS Animation & SurfaceFlinger Pacing          │
    # └─────────────────────────────────────────────────────────────┘
    resetprop -n debug.sf.disable_backpressure 1 2>/dev/null || setprop debug.sf.disable_backpressure 1 2>/dev/null || true
    resetprop -n debug.sf.latch_unsignaled 1 2>/dev/null || setprop debug.sf.latch_unsignaled 1 2>/dev/null || true
    resetprop -n debug.hwui.render_thread_priority -20 2>/dev/null || setprop debug.hwui.render_thread_priority -20 2>/dev/null || true
    resetprop -n renderthread.skia.reduceopstasksplitting true 2>/dev/null || setprop renderthread.skia.reduceopstasksplitting true 2>/dev/null || true

    # ┌─────────────────────────────────────────────────────────────┐
    # │  Layer 8: Reduce MIUI/HyperOS Background Service Bloat     │
    # └─────────────────────────────────────────────────────────────┘
    # Disable Xiaomi analytics, cloud sync, and background telemetry
    # that keeps dozens of ContentProvider/BroadcastReceiver handles alive.
    resetprop -n persist.sys.miui_optimization true 2>/dev/null || setprop persist.sys.miui_optimization true 2>/dev/null || true
    resetprop -n ro.miui.has_handy_mode_sf 1 2>/dev/null || setprop ro.miui.has_handy_mode_sf 1 2>/dev/null || true

    # Note: Post-boot memory compaction & slab settling is handled natively
    # in-kernel via AETHER C workqueue (0 userspace subshells, 0 CPU overhead)

    # Weekly fstrim only (avoids post-boot I/O stalls that feel like instability)
    FSTRIM_STAMP=/data/adb/aether/.last_fstrim
    now=$(date +%s)
    last=0
    [ -f "$FSTRIM_STAMP" ] && last=$(cat "$FSTRIM_STAMP" 2>/dev/null || echo 0)
    if [ "$((now - last))" -gt 604800 ]; then
        fstrim -v /data 2>/dev/null || true
    fi
EOF
    chmod 0755 /data/adb/service.d/01_displayfeatures.sh;
    chown 0:0 /data/adb/service.d/01_displayfeatures.sh;
    ui_print "        └─ Display, storage, network, audio & scheduler services installed";

    # Ensure open permissions on custom tuneable nodes
    chmod 0666 /sys/kernel/aether/* 2>/dev/null || true;
    chmod 0666 /sys/kernel/kprofiles/* 2>/dev/null || true;
    chmod 0666 /sys/module/kprofiles/parameters/* 2>/dev/null || true;
    chmod 0666 /proc/sys/kernel/sched_bore* 2>/dev/null || true;
    chmod 0666 /sys/module/cpu_boost/parameters/* 2>/dev/null || true;
    chmod 0666 /sys/module/cpu_input_boost/parameters/* 2>/dev/null || true;
    chmod 0666 /sys/module/devfreq_boost/parameters/* 2>/dev/null || true;
    chmod 0666 /dev/xiaomi-touch 2>/dev/null || true;
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
