<p align="center">
  <img src="assets/banner.svg" alt="AETHER Kernel Banner" width="100%">
</p>

<p align="center">
  <a href="https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/latest"><img src="https://img.shields.io/badge/Release-v1.2-38bdf8?style=flat-square&logo=github" alt="Release"></a>
  <a href="#"><img src="https://img.shields.io/badge/Kernel-Linux%204.14.356-818cf8?style=flat-square&logo=linux" alt="Kernel"></a>
  <a href="#"><img src="https://img.shields.io/badge/Device-mojito%20%2F%20sunny-c084fc?style=flat-square&logo=android" alt="Device"></a>
  <a href="#"><img src="https://img.shields.io/badge/SoC-Qualcomm%20SM6150-34d399?style=flat-square&logo=qualcomm" alt="SoC"></a>
  <a href="#"><img src="https://img.shields.io/badge/KernelSU-v3.3.1%20(uapi%205)-f472b6?style=flat-square" alt="KernelSU"></a>
  <a href="#"><img src="https://img.shields.io/badge/License-GPL--2.0-94a3b8?style=flat-square" alt="License"></a>
</p>

---

## Overview

**AETHER Kernel** is an open-source, custom Linux kernel project built specifically for the **Xiaomi Redmi Note 10 (mojito / sunny)** powered by the **Qualcomm Snapdragon 678 (SM6150)**.

Engineered with a focus on stability, fluid multitasking, and clean human-written code, AETHER brings genuine modern memory management (MGLRU), native KernelSU integration, calibrated in-call audio, and full display panel controls to Android 14, 15, and 16 ROMs.

---

## Key Features

### 🧠 Multi-Generational LRU (MGLRU)
* Genuine Google upstream MGLRU implementation (`CONFIG_LRU_GEN=y`).
* Classifies memory into generational tiers using hardware access bits rather than the legacy two-list queue.
* Eliminates aggressive launcher redraws, app reloads, and foreground stutter under memory pressure.

### 🛡️ KernelSU-Next Built-in (v3.3.1)
* Synced with `backslashxx/KernelSU` upstream at tag `v3.3.1` (UAPI v5 / versionCode `32680`).
* Dynamic UAPI handshake preventing "Kernel update required" mismatch warnings.
* Out-of-the-box support for full module flashing, su mounts, and root isolation.
* Bundled official `ksud` binary provisioned directly during recovery installation.

### ☀️ Display & Panel Features
* Full kernel sysfs permissions configured for companion modules:
  - **DC Dimming**: `/sys/.../soc:qcom,dsi-display-lab/anti_flicker`
  - **High Brightness Mode (HBM)**: `/sys/.../soc:qcom,dsi-display-lab/hbm`
  - **Real-time FPS tracking**: `/sys/.../drm/card0/sde-crtc-0/measured_fps`
* Tested and verified with `DisplayFeatures-Module-mojito-A16-infinityX`.

### ⚡ Performance & Scheduler Tuning
* **300Hz Timer Frequency**: Perfectly synchronizes frame tick intervals without unnecessary battery drain.
* **Schedutil Governor Optimization**: Fast frequency ramp up (500µs) and calibrated ramp down (20ms) for instantaneous UI response.
* **Direct Reclaim Isolation**: Watermark scale factor set to `150` with throttled direct swappiness (`20`) to prevent UI lockups.
* **512KB UFS Read-Ahead**: Maximizes sequential read throughput on UFS 2.2 storage for rapid cold app startups.

---

## Kernel Editions

| Edition | In-Call Microphone | Target Use-Case |
| :--- | :--- | :--- |
| **MicFix Edition** | Calibrated routing & un-mute | Recommended for ROMs experiencing in-call audio dropouts or microphone issues. |
| **Standard Edition** | Stock Qualcomm drivers | Clean baseline for ROMs where stock in-call microphone works normally. |

---

## Downloads

Official flashable zip archives and companion packages are available on the [GitHub Releases](https://github.com/kiran-embedded/AETHER-Kernel-mojito/releases/latest) page:

* **`AETHER-Kernel-mojito-v1.2-MicFix.zip`** — Flashable AnyKernel3 package with calibrated in-call audio.
* **`AETHER-Kernel-mojito-v1.2-Standard.zip`** — Flashable AnyKernel3 package with stock audio.
* **`KernelSU_v3.3.1_32680-release.apk`** — Matching KernelSU Manager application.
* **`DisplayFeatures-Module-mojito-A16-infinityX.zip`** — Magisk / KSU module for DC Dimming & HBM tiles.
* **`KProfiles-Module-mojito-A16-infinityX.zip`** — Kernel power mode management module.

---

## Installation

### Prerequisites
* Unlocked bootloader on Xiaomi Redmi Note 10 (mojito / sunny).
* Custom Recovery installed (**TWRP** or **OrangeFox**).
* Any compatible AOSP / Custom ROM (Android 14, 15, or 16).

### Flashing Steps
1. Reboot into recovery (TWRP / OrangeFox).
2. *(Optional but recommended)* Take a backup of your existing `Boot` and `DTBO` partitions.
3. Tap **Install** and select your desired zip:
   - `AETHER-Kernel-mojito-v1.2-MicFix.zip` OR `AETHER-Kernel-mojito-v1.2-Standard.zip`
4. Swipe to confirm flash and watch the installation progress.
5. Reboot to System.
6. Install `KernelSU_v3.3.1_32680-release.apk` to manage root privileges and flash modules.

---

## Verification

Once booted into AETHER Kernel, verify features via Termux or ADB shell:

```bash
# Check kernel version and localversion
uname -r

# Verify active MGLRU state
cat /sys/kernel/mm/lru_gen/enabled

# Check DC Dimming node
ls -l /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/anti_flicker
```

---

## Credits & Acknowledgments

* **Lead Developer & Maintainer**: [Kiran_Embedded](https://github.com/kiran-embedded)
* **AnyKernel3**: [osm0sis](https://github.com/osm0sis/AnyKernel3)
* **KernelSU**: [backslashxx](https://github.com/backslashxx/KernelSU) & [KernelSU Team](https://github.com/tiann/KernelSU)
* **Linux Kernel Community & Qualcomm Innovation Center**

---

<p align="center">
  <sub>Crafted by Kiran_Embedded • Xiaomi Redmi Note 10 Community</sub>
</p>
