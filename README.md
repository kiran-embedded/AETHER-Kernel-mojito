<p align="center">
  <img src="assets/banner.svg" alt="AETHER Kernel Banner" width="100%">
</p>

<p align="center">
  <a href="https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/latest"><img src="https://img.shields.io/badge/Release-v1.4%20Stable-38bdf8?style=flat-square&logo=github" alt="Release"></a>
  <a href="#"><img src="https://img.shields.io/badge/Kernel-Linux%204.14.356-818cf8?style=flat-square&logo=linux" alt="Kernel"></a>
  <a href="#"><img src="https://img.shields.io/badge/Device-mojito%20%2F%20sunny-c084fc?style=flat-square&logo=android" alt="Device"></a>
  <a href="#"><img src="https://img.shields.io/badge/SoC-Qualcomm%20SM6150-34d399?style=flat-square&logo=qualcomm" alt="SoC"></a>
  <a href="https://gitlab.com/simonpunk/susfs4ksu"><img src="https://img.shields.io/badge/SuSFS-v1.5.5-10b981?style=flat-square" alt="SuSFS"></a>
  <a href="https://github.com/backslashxx/KernelSU"><img src="https://img.shields.io/badge/KernelSU-backslashxx%20Supported-f43f5e?style=flat-square" alt="KernelSU"></a>
  <a href="#"><img src="https://img.shields.io/badge/License-GPL--2.0-94a3b8?style=flat-square" alt="License"></a>
</p>

---

## Overview

**AETHER Kernel** is an open, community-driven custom Linux kernel built for the **Xiaomi Redmi Note 10 (mojito / sunny)** powered by the **Qualcomm Snapdragon 678 (SM6150)**.

Engineered for daily driver stability, smooth multitasking, reliable fast charging, and zero-compromise root concealment. Compatible with Android 14, 15, and 16 custom ROMs.

---

## Table of Contents
1. [Core Highlights](#core-highlights)
2. [Charging Subsystem Architecture](#charging-subsystem-architecture)
3. [Google Developer Tweaks & I/O Pipeline](#google-developer-tweaks--io-pipeline)
4. [BORE CPU Scheduler & Multi-Gen LRU](#bore-cpu-scheduler--multi-gen-lru)
5. [SuSFS v1.5.5 Stealth & Kernel Uname Spoofing](#susfs-v155-stealth--kernel-uname-spoofing)
6. [Standard vs Enhanced Editions](#standard-vs-enhanced-editions)
7. [Recommended KernelSU Managers](#recommended-kernelsu-managers)
8. [Downloads & Package Checksums](#downloads--package-checksums)
9. [Installation Guide](#installation-guide)
10. [On-Device Verification & Proof Logs](#on-device-verification--proof-logs)
11. [Source Code, GPLv2 License & Credits](#source-code-gplv2-license--credits)

---

## Core Highlights

* **Charging Freeze Elimination**: Resolves the algorithm freeze where charging stalled at 0mA–200mA at 70%–80% battery via the new `PD_PM_STATE_SW_LOOP` fallback.
* **Working Screen-On Fast Charging**: Clamped screen-on thermal throttling to Level 8 fast charge ceiling when battery temp is below 45°C.
* **Third-Party PD / PPS Charger Compatibility**: Lowered minimum adapter threshold from 2000mA to 1500mA so 18W, 20W, and 33W non-stock chargers negotiate cleanly.
* **Google Developer Storage & Kernel Tweaks**: Google Kyber BLK-MQ I/O scheduler, 256KB/512KB UFS read-ahead, SchedTune `top-app` idle core preference, TCP lowat socket pacing, and balanced dirty page writeback.
* **Dual KernelSU Support (backslashxx & Upstream)**: Built with [backslashxx/KernelSU](https://github.com/backslashxx/KernelSU) support, while also handling official [tiann/KernelSU](https://github.com/tiann/KernelSU) and [rifsxd/KernelSU-Next](https://github.com/rifsxd/KernelSU-Next) APKs without UAPI version mismatch warnings.
* **SuSFS v1.5.5 Built-In**: Full kernel-level stealth with hidden kallsyms symbols (`CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS`) and live kernel release spoofing to stock `4.14.190-perf+`.
* **BORE Scheduler v5.1.0**: CFS Burst-Oriented Response Enhancer tuned for touch burst prioritization.
* **Multi-Gen LRU (MGLRU)**: Upstream generational page access tracking (`0x0003`) prevents aggressive app kills and launcher redraws.
* **Audio Route Bypass by Default**: Audio hardware registers (ADC gains, decimators, HPF filters) remain 100% factory stock Qualcomm unless customized via the companion WebUI.
* **WebUI Freeze Resolved**: Replaced heavy `/proc/kallsyms` table parsing in the control panel with instant 3ms sysfs queries.

---

## Charging Subsystem Architecture

### The Problem
On stock and early custom kernels, Redmi Note 10 users frequently encountered two issues:
1. When battery capacity reached 70%–80%, charging current often dropped to near zero (0.0mA–200mA) and stayed frozen there.
2. Turning the screen on while charging caused thermal algorithms to aggressively throttle charging current down to snail speeds, even if the device was physically cool.
3. Third-party USB Power Delivery (PD) and PPS chargers (18W, 20W, 33W) often got stuck in rapid connect/disconnect handoff loops.

### The Engineering Fix
* **TI Charge Pump Taper Loop Fallback (`PD_PM_STATE_SW_LOOP`)**:
  In `drivers/power/supply/ti/pd_policy_manager.c`, the state machine previously lacked a safe handoff when the charge pump finished its taper phase (`PM_ALGO_RET_TAPER_DONE`). We added `PD_PM_STATE_SW_LOOP` into `enum pm_state`. When the taper completes, the algorithm switches cleanly into switching charging mode without deadlock.
* **1500mA Adapter Floor**:
  Decreased `.min_adapter_curr_required` from 2000mA to 1500mA in `pm_config`. Non-stock 18W–33W PPS power bricks that advertise 1.5A–2.2A rails now negotiate instantly.
* **Balanced Screen-On Thermal Clamping**:
  In `drivers/power/supply/qcom/smb5-lib.c` and `drivers/power/supply/qcom/qpnp-smb5.c`, the thermal daemon's screen-on throttling is clamped to **Level 8** (allowing $\ge 3.8\text{A}$ charging current) as long as battery temperature remains below **45°C**.
* **Safety Intact**:
  A non-negotiable hardware safety cutoff is enforced at **46°C**. If battery temperature ever hits 46°C, charging current is immediately cut to protect battery health.

---

## Google Developer Tweaks & I/O Pipeline

To maximize smoothness on the Redmi Note 10's UFS 2.2 storage and Snapdragon 678 SoC, we integrated battle-tested performance tweaks:

### 1. Google Kyber BLK-MQ I/O Scheduler
* Replaced outdated single-queue storage schedulers with **Kyber**, Google's multi-queue scheduler engineered specifically for flash storage.
* Kyber actively monitors request completion latencies:
  * Target read latency: **2ms**
  * Target write latency: **10ms**
* If write operations (such as app installations or file downloads) begin to cause storage congestion, Kyber temporarily throttles background write dispatching to guarantee that foreground UI read requests complete instantly.

### 2. UFS Storage Read-Ahead Cache
* Configured `/sys/block/sd*/queue/read_ahead_kb` to **256KB** (with 512KB peak tuning available).
* Provides enough prefetch buffer for rapid cold app startups without wasting memory on unwanted data chunks.

### 3. SchedTune Foreground Prioritization
* Configured `/dev/stune/top-app`:
  * `schedtune.prefer_idle = 1`: Instructs the CPU scheduler to place UI and touch tasks directly onto idle CPU cores rather than packing them onto already-busy cores.
  * `schedtune.boost = 10`: Gives interactive threads a slight initial frequency boost upon touch down for immediate frame rendering.

### 4. TCP Lowat & Dirty Memory Pacing
* `tcp_notsent_lowat = 16384`: Avoids buffer bloat in network sockets, keeping voice and multiplayer packets lean and low-latency.
* `tcp_fastopen = 3`: Enables TCP Fast Open for both client and server handshakes.
* `tcp_congestion_control = bbr`: Replaces legacy cubic with Google BBR for improved throughput over unstable cellular and Wi-Fi networks.
* Virtual Memory tuning:
  * `vm.dirty_ratio = 20`
  * `vm.dirty_background_ratio = 10`
  * `vm.dirty_writeback_centisecs = 1000`
  * `vm.dirty_expire_centisecs = 2000`
  * Prevents the OS from accumulating excessive unwritten cache in RAM, eliminating background write freezes.

---

## BORE CPU Scheduler & Multi-Gen LRU

### BORE Scheduler (Burst-Oriented Response Enhancer v5.1.0)
Authored by Masahito Suzuki (CachyOS) and backported to Linux 4.14:
* Standard CFS treats all CPU tasks equally based on historic runtime. BORE introduces burst-time tracking.
* Short, bursty tasks (touch input events, UI gestures, audio callbacks) receive immediate scheduling priority.
* Continuous, non-yielding background workloads are gently deprioritized to prevent them from inducing frame drops.
* Verified active via sysctl: `sched_bore = 1`, `sched_burst_penalty_scale = 1280`.

### Multi-Generational LRU (MGLRU)
* Genuine Google upstream implementation (`CONFIG_LRU_GEN=y`).
* Traditional Linux memory management relies on a simplistic two-list (Active/Inactive) queue that easily loses track of which pages are actually hot under memory pressure.
* MGLRU uses hardware page-table access bits to organize memory pages into multi-generational age tiers.
* The kernel evicts truly cold pages first, keeping your launcher, keyboard, and messaging apps alive in RAM without thrashing kswapd. Verified active via sysfs: `0x0003`.

---

## SuSFS v1.5.5 Stealth & Kernel Uname Spoofing

### Built-in SuSFS Integration
* Version: **v1.5.5 (NON-GKI)**.
* Integrated with `CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=y`:
  * Conceals SuSFS and KernelSU internal hook symbols from `/proc/kallsyms`.
  * Regular apps, banking checks, and automated root scanners querying kallsyms find zero traces of kernel rooting.

### Live Kernel Release Spoofing
* Many detection engines (Momo, Native Test tools, banking security SDKs) inspect `uname()` release strings to spot custom kernels.
* Through the companion spoofer module, SuSFS dynamically spoofs the kernel release:
  ```bash
  $ uname -r
  4.14.190-perf+
  
  $ uname -a
  Linux localhost 4.14.190-perf+ #91 SMP PREEMPT Thu Oct 15 14:41:47 IST 2026 aarch64 Toybox
  ```
* Critical system daemons like `bpfloader` and `netd` are guarded in-kernel to ensure networking and eBPF run with zero boot hang.

---

## Standard vs Enhanced Editions

| Feature / Metric | Standard Edition | Enhanced Edition |
| :--- | :--- | :--- |
| **Primary Use Case** | **Recommended for most users** (Clean, balanced daily driver) | **Recommended for ROMs with in-call mic delay or low call volume** |
| **Audio Driver Base** | 100% Stock Qualcomm (`techpack/audio` untouched) | Driver-level low-latency audio patch applied |
| **Call Pickup Unmute Delay** | Stock 40 ms (`system_power_efficient_wq`) | **5 ms instant unmute** on `system_highpri_wq` |
| **Call Answer Silence Bug** | May experience 1–2s silence on some custom ROMs | **Eliminates the 1–2 second mute delay on call answer** |
| **Hardware HPF Cutoff** | Stock 150 Hz (`CF_MIN_3DB_150HZ`) | **75 Hz (`CF_MIN_3DB_75HZ`)** for fuller vocal richness |
| **Analog Pre-Amp Gain** | Stock `0x20` | Boosted `0x24` on `WCD937X_ANA_TX_CH1` |
| **Charging Fix (No 70–80% freeze)** | Included | Included |
| **Screen-On Fast Charging** | Included | Included |
| **Google Kyber I/O & Tweaks** | Included | Included |
| **SuSFS v1.5.5 & Uname Disguise** | Included | Included |
| **BORE Scheduler v5.1.0** | Included | Included |
| **MGLRU Memory Engine** | Included | Included |
| **Dual KSU / KSU-Next Support** | Included | Included |

---

## Recommended KernelSU Managers

AETHER Kernel v1.4 handles UAPI handshakes dynamically, giving you the freedom to choose your manager app:

* 🥇 **[backslashxx KernelSU Manager](https://github.com/backslashxx/KernelSU/releases)** *(Primary Recommended)*:
  Maintained by backslashxx, perfectly aligned with our kernel UAPI implementation. Clean, stable, and zero overhead.
* 🥈 **[Official KernelSU Manager](https://github.com/tiann/KernelSU/releases)**:
  The official upstream manager by tiann and the KernelSU team.
* 🥉 **[KernelSU-Next Manager](https://github.com/rifsxd/KernelSU-Next/releases)**:
  Modern interface with extended module configuration features.

---

## Downloads & Package Checksums

Download all flashable packages and companion modules from the [GitHub v1.4 Release](https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/tag/v1.4):

| File Name | Description | Size | SHA-256 Checksum |
| :--- | :--- | :--- | :--- |
| **AETHER-Kernel-mojito-v1.4-Standard.zip** | Flashable AnyKernel3 package (Standard) | ~16.6 MB | `85376155a460b95bc248c2b5c8e54b34b8784852814cef6fc75481b29449c3a9` |
| **AETHER-Kernel-mojito-v1.4-Enhanced.zip** | Flashable AnyKernel3 package (Enhanced) | ~16.6 MB | `b083aac1b2671b7e0cf2306c8bf3d140b61140585c298bb286866e2a63a8d005` |
| **AETHER-Kernel-Control-WebUI-v1.4.0.zip** | Companion WebUI control module | ~51 KB | `ae122f84baac5a39489ebd51bb1bc7af14518990f1a98f95d94155513013aa0e` |
| **AETHER-SuSFS-Stock-Spoofer.zip** | SuSFS automated disguise module | ~126 KB | `790814d7287dc362df1b296e8bffae9361e1bfdb5bfbac556e7cc99bf97b2789` |
| **ksu_module_susfs.zip** | SuSFS core companion module | ~127 KB | `0af2bb65fcb10179f0bbe41edaccf2348e98fe949130fcd4c82f9db32af53f21` |
| **sha256sum.txt** | Verified package checksum file | 499 B | `363750907864fdde35f1192e2bb4a8d052608404a0cb6c52a32c25368a5146c6` |

---

## Installation Guide

### Prerequisites
* Unlocked bootloader on Xiaomi Redmi Note 10 (`mojito` or `sunny`).
* Custom recovery installed (**TWRP** or **OrangeFox**).
* Any compatible Android 14, 15, or 16 custom ROM (AOSP, LineageOS, crDroid, InfinityX, HyperOS ports).

### Flashing Procedure
1. Boot into custom recovery (**TWRP** or **OrangeFox**).
2. *(Good practice)* Create a quick NANDroid backup of your current `Boot` and `DTBO` partitions.
3. Tap **Install** and select either:
   * `AETHER-Kernel-mojito-v1.4-Standard.zip` *(recommended for most users)*
   * `AETHER-Kernel-mojito-v1.4-Enhanced.zip` *(if experiencing in-call mic delay)*
4. Swipe to confirm flash.
5. Reboot to System.
6. Install your preferred manager APK ([backslashxx KernelSU](https://github.com/backslashxx/KernelSU/releases), [Official KSU](https://github.com/tiann/KernelSU/releases), or [KernelSU-Next](https://github.com/rifsxd/KernelSU-Next/releases)).
7. Open the manager app and grant root permissions when prompted.
8. In the Modules tab, flash:
   * `AETHER-Kernel-Control-WebUI-v1.4.0.zip`
   * `AETHER-SuSFS-Stock-Spoofer.zip`
9. Reboot once more to allow the background spoofer service to settle.

---

## On-Device Verification & Proof Logs

Captured directly from an active test run on a live Xiaomi Redmi Note 10 (`sunny`):

```bash
# 1. Active Kernel Version & Localversion Banner
$ uname -a
Linux localhost 4.14.356-AETHER-v1.4-Kiran_Embedded #91 SMP PREEMPT Thu Oct 15 14:41:47 IST 2026 aarch64 Toybox

# 2. Charging Handshake & Thermal State Under Active USB-PD Fast Charge
$ cat /sys/class/power_supply/battery/status
Charging
$ cat /sys/class/power_supply/battery/charge_type
Fast
$ cat /sys/class/power_supply/battery/temp
353  (35.3°C — optimal thermal range)
$ cat /sys/class/power_supply/battery/voltage_now
4172628  (4.172 V)
$ cat /sys/class/power_supply/battery/charge_control_limit
8  (Level 8 thermal clamp active)

# 3. Kernel SuSFS Status & Live Uname Disguise
$ /data/adb/modules/aether_susfs_spoofer/tools/ksu_susfs_arm64 show version
v1.5.5
$ /data/adb/modules/aether_susfs_spoofer/tools/ksu_susfs_arm64 show variant
NON-GKI
$ uname -r
4.14.190-perf+  (Disguised as stock Xiaomi kernel string)

# 4. BORE Scheduler Sysctl Tuning
$ cat /proc/sys/kernel/sched_bore
1
$ cat /proc/sys/kernel/sched_burst_penalty_scale
1280
$ cat /proc/sys/kernel/sched_burst_cache_lifetime
60000000

# 5. Multi-Gen LRU (MGLRU) Active State
$ cat /sys/kernel/mm/lru_gen/enabled
0x0003

# 6. Adreno 612 GPU Governor & Clock Scaling
$ cat /sys/class/kgsl/kgsl-3d0/devfreq/governor
msm-adreno-tz
$ cat /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq
550000000  (550 MHz dynamic step)

# 7. Stock Factory Audio Registers Baseline (Untouched)
$ tinymix "ADC1 Volume"
ADC1 Volume: 13 (dsrange 0->20)
$ tinymix "TX_DEC0 Volume"
TX_DEC0 Volume: 78 (dsrange 0->124)
$ tinymix "TX0 HPF cut off"
TX0 HPF cut off: >CF_NEG_3DB_150HZ
```

---

## Source Code, GPLv2 License & Credits

### Source Code Transparency & Licensing
* AETHER Kernel is licensed under the **GNU General Public License v2 (GPL-2.0)** in accordance with the Linux kernel license.
* The full kernel source tree is actively maintained, tested, and organized in our private development branch.
* In strict compliance with GPLv2, anyone who wishes to inspect the source tree, review the patch sets, or reproduce kernel builds is welcome to open an issue on the repository or reach out to request the full tree.

### Credits & Upstream Community
Huge appreciation to the open-source developers whose work makes this project possible:
* **AnyKernel3**: [osm0sis/AnyKernel3](https://github.com/osm0sis/AnyKernel3)
* **KernelSU**: [backslashxx/KernelSU](https://github.com/backslashxx/KernelSU) & [tiann/KernelSU](https://github.com/tiann/KernelSU)
* **KernelSU-Next**: [rifsxd/KernelSU-Next](https://github.com/rifsxd/KernelSU-Next)
* **SuSFS (4KSU)**: [simonpunk/susfs4ksu](https://gitlab.com/simonpunk/susfs4ksu) & [YuKongA](https://github.com/YuKongA)
* **BORE CPU Scheduler**: [firelzrd/bore](https://github.com/firelzrd/bore) & Masahito Suzuki (CachyOS)
* **CPU Boost & Kernel Optimizations**: [Sultan Alsawaf (kerneltoast)](https://github.com/kerneltoast)
* **Qualcomm & Xiaomi**: For the base SM6150 kernel tree and hardware drivers.

---

<p align="center">
  <sub>Maintained by Kiran_Embedded • Xiaomi Redmi Note 10 Community</sub>
</p>
