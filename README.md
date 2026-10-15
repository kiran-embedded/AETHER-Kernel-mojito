<p align="center">
  <img src="assets/banner.svg" alt="AETHER Kernel Banner" width="100%">
</p>

<p align="center">
  <a href="https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/latest"><img src="https://img.shields.io/badge/Release-v1.4%20Stable-38bdf8?style=flat-square&logo=github" alt="Release"></a>
  <a href="#"><img src="https://img.shields.io/badge/Kernel-Linux%204.14.356-818cf8?style=flat-square&logo=linux" alt="Kernel"></a>
  <a href="#"><img src="https://img.shields.io/badge/Device-mojito%20%2F%20sunny-c084fc?style=flat-square&logo=android" alt="Device"></a>
  <a href="#"><img src="https://img.shields.io/badge/SoC-Qualcomm%20SM6150-34d399?style=flat-square&logo=qualcomm" alt="SoC"></a>
  <a href="#"><img src="https://img.shields.io/badge/SuSFS-v1.5.5-10b981?style=flat-square" alt="SuSFS"></a>
  <a href="#"><img src="https://img.shields.io/badge/License-GPL--2.0-94a3b8?style=flat-square" alt="License"></a>
</p>

---

## Overview

**AETHER Kernel** is a custom Linux kernel built for the **Xiaomi Redmi Note 10 (mojito / sunny)** powered by the **Qualcomm Snapdragon 678 (SM6150)**.

Designed for day-to-day stability, fluid multitasking, reliable charging, and zero-compromise root hiding. Compatible with Android 14, 15, and 16 custom ROMs.

---

## What's Inside

### 🔋 Rock-Solid Charging System
* **70%-80% Charging Freeze Fixed**: Rewrote the TI charge pump state machine transition (`PD_PM_STATE_SW_LOOP`). Once your battery crosses 75%, it seamlessly switches modes instead of freezing at 0mA–200mA.
* **Functional Screen-On Charging**: Clamped screen-on thermal throttling to level 8 fast charge ceiling when battery temp is below 45°C. The phone actually charges while you use it without overheating.
* **Third-Party PPS / PD Support**: Lowered minimum adapter requirement threshold from 2000mA to 1500mA so 18W, 20W, and 33W non-stock chargers negotiate cleanly without cycling connect/disconnect loops.
* **Hardware Safety**: Strict 46°C battery temp cutoff remains fully enforced.

### 🛡️ Kernel-Level SuSFS v1.5.5 Stealth
* Full SuSFS v1.5.5 integration with `CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS` enabled.
* All KernelSU and SuSFS symbols are concealed from `/proc/kallsyms`.
* Live kernel release spoofing disguises the kernel string as official stock `4.14.190-perf+` to pass sensitive banking apps and Play Integrity checks.

### ⚡ BORE Scheduler (v5.1.0)
* Burst-Oriented Response Enhancer baked directly into the CFS task scheduler.
* Dynamically identifies and prioritizes touch/UI bursts so app scrolling stays smooth even during background app syncing.

### 🧠 Multi-Generational LRU (MGLRU)
* Genuine Google MGLRU (`CONFIG_LRU_GEN=y`) actively sorting page access tiers (`0x0003`).
* Prevents unnecessary app reloads, keeping your open apps alive in RAM without battery drain.

### 🎙️ Factory Stock Audio by Default
* Audio registers (ADC pre-amps, digital decimators, HPF filters, unmute workqueues) remain 100% factory stock Qualcomm on install.
* Zero in-call mic issues or weird voice call levels out of the box. Custom tuning can be unlocked manually inside the WebUI panel whenever desired.

---

## Kernel Editions

| Edition | Description | Recommended For |
| :--- | :--- | :--- |
| **Standard Edition** | Balanced daily driver, factory stock audio baseline, thermal safety clamped to level 8. | Daily use, maximum stability, all ROMs. |
| **Enhanced Edition** | Slightly higher thermal headroom and aggressive scheduler scaling for heavier usage. | Heavy multitasking and gaming. |

---

## Downloads & Assets (v1.4 Stable)

Get the latest zip packages from the [GitHub v1.4 Release](https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/tag/v1.4):

| File Name | Description | Direct Download |
| :--- | :--- | :--- |
| **AETHER-Kernel-mojito-v1.4-Standard.zip** | AnyKernel3 flashable kernel (Standard) | [⬇️ Download ZIP](https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/download/v1.4/AETHER-Kernel-mojito-v1.4-Standard.zip) |
| **AETHER-Kernel-mojito-v1.4-Enhanced.zip** | AnyKernel3 flashable kernel (Enhanced) | [⬇️ Download ZIP](https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/download/v1.4/AETHER-Kernel-mojito-v1.4-Enhanced.zip) |
| **AETHER-Kernel-Control-WebUI-v1.4.0.zip** | WebUI Control Panel module (KSU / APatch / Magisk) | [⬇️ Download Module](https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/download/v1.4/AETHER-Kernel-Control-WebUI-v1.4.0.zip) |
| **AETHER-SuSFS-Stock-Spoofer.zip** | SuSFS automated bootloader & uname disguise module | [⬇️ Download Module](https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/download/v1.4/AETHER-SuSFS-Stock-Spoofer.zip) |
| **ksu_module_susfs.zip** | SuSFS core helper module | [⬇️ Download Module](https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/download/v1.4/ksu_module_susfs.zip) |
| **sha256sum.txt** | Verified SHA256 package checksums | [⬇️ Download Checksums](https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/download/v1.4/sha256sum.txt) |

---

## Installation

1. Boot into custom recovery (**TWRP** or **OrangeFox**).
2. *(Recommended)* Make a quick backup of your existing `Boot` and `DTBO` partitions.
3. Flash `AETHER-Kernel-mojito-v1.4-Standard.zip` (or `Enhanced.zip`).
4. Reboot into Android.
5. In KernelSU (or APatch / Magisk), install `AETHER-Kernel-Control-WebUI-v1.4.0.zip` and `AETHER-SuSFS-Stock-Spoofer.zip`.
6. Reboot once more to allow the spoofer service to apply cleanly.

---

## Verification & Proof

You can verify all features right from Termux or an ADB shell:

```bash
# Check running kernel version
uname -a

# Verify SuSFS uname spoofing
uname -r
# Output: 4.14.190-perf+

# Verify BORE scheduler is active
cat /proc/sys/kernel/sched_bore
# Output: 1

# Verify MGLRU active state
cat /sys/kernel/mm/lru_gen/enabled
# Output: 0x0003

# Verify live battery temperature & charging status
cat /sys/class/power_supply/battery/status
cat /sys/class/power_supply/battery/temp
```

---

## Source Code & Licensing

* This project is licensed under the **GNU General Public License v2 (GPL-2.0)** in accordance with the Linux kernel license.
* The complete kernel source code is actively maintained and organized in our private development branch. In strict compliance with GPL-2.0, anyone interested in inspecting the source tree, patch sets, or reproducing builds is welcome to open an issue or reach out to request the full tree.

---

## Credits & Thanks

* **osm0sis** for AnyKernel3.
* **Sultan Alsawaf** for CPU input boost and kernel optimization work.
* **Masahito Suzuki & CachyOS team** for the BORE scheduler.
* **simonpunk & YuKongA** for SuSFS.
* **backslashxx & KernelSU Team** for KernelSU.
* **Qualcomm & Xiaomi** for the SM6150 base tree.

---

<p align="center">
  <sub>Maintained by Kiran_Embedded • Xiaomi Redmi Note 10 Community</sub>
</p>
