# AETHER Kernel v1.6.1 — Xiaomi Redmi Note 10 (mojito / sunny)

[![Platform](https://img.shields.io/badge/Android-16%20%7C%2017-blue.svg?style=flat-square)](https://github.com/kiran-embedded/AETHER-Kernel-mojito)
[![Kernel](https://img.shields.io/badge/Kernel-4.14.356-success.svg?style=flat-square)](https://github.com/kiran-embedded/AETHER-Kernel-mojito)
[![SoC](https://img.shields.io/badge/SoC-Qualcomm%20Snapdragon%20678%20(SM6150)-orange.svg?style=flat-square)](https://github.com/kiran-embedded/AETHER-Kernel-mojito)
[![KernelSU-Next](https://img.shields.io/badge/KernelSU--Next-v1.0.5%20(uapi%205)-purple.svg?style=flat-square)](https://github.com/backslashxx/KernelSU)
[![SuSFS](https://img.shields.io/badge/SuSFS-v2.3.0-10b981.svg?style=flat-square)](https://gitlab.com/simonpunk/susfs4ksu)
[![License](https://img.shields.io/badge/License-GPL--2.0-lightgrey.svg?style=flat-square)](LICENSE)

A high-performance, ring-0 optimized custom Linux kernel built specifically for the **Xiaomi Redmi Note 10 (`mojito` / `sunny`)** running **Android 16 and Android 17** (AOSP, LineageOS, PixelOS, and custom ROMs).

---

## 🚀 What's New in v1.6.1

### 🔋 1. Smart Charging & Ultra-Safe Thermal Knots
* **Fixed 33W Mi Turbo Charge Handshake**: Resolved the issue where Turbo Charging would collapse to 5mA–100mA after connecting. Native 33W Xiaomi Turbo Charge now engages reliably and ramps up smoothly across 10W, 18W, and 33W chargers.
* **Ultra-Safe Battery Thermal Curve**: Implemented fine-grained thermal knots with linear interpolation for smooth thermal management:
  - `< 10.0°C`: OEM JEITA Cold Protection
  - `10.0°C – 38.0°C`: **6.0A Max** (Full 33W Turbo unconstrained)
  - `38.0°C – 39.0°C`: **5.5A – 6.0A** (Begin thermal reduction)
  - `39.0°C – 40.0°C`: **4.5A** (Smooth reduction)
  - `40.0°C – 41.0°C`: **3.5A** (Warm protection)
  - `41.0°C – 42.0°C`: **2.5A** (Strong reduction)
  - `42.0°C – 43.0°C`: **1.5A** (High-temperature protection)
  - `43.0°C – 44.0°C`: **0.8A** (Minimal charging / trickle)
  - `≥ 45.0°C`: **0.0A** (Emergency thermal suspend; auto-recovers at `≤ 44.0°C` with 1.0°C deadband)
* **Fluid Current Slew-Rate Limiting**: Rate-limited step-down (`200 mA/s`) and gradual restore (`150 mA/s`) prevent sudden voltage drops, thermal hunting, or rapid current fluttering.
* **100% Target Cut-off & Hardware Bypass**: Completes full 33W CC phase and natural CV taper to 100%, then transitions into True Hardware Bypass (0mA net battery draw, 3.0A USB-ICL motherboard direct power).

### ⚡ 2. Performance & Deep Sleep Optimization
* **Zero Idle Drain**: 0 active wake locks held during screen-off. Background monitoring pauses completely when disconnected, allowing the SoC to enter deep PC3/AOSS sleep.
* **Upstream MGLRU & Tuned Watermarks**: Multi-Gen LRU enabled with proactive page cleaning to prevent background app kills during heavy multitasking.
* **Two-Stage Boot Booster**: 1024KB boot burst scales down to 384KB steady-state 15 seconds after unlock for fast app launching without excess memory usage.

### 🎧 3. Audio & Display Enhancements
* **Pro Audio Controls**: Mic HPF noise suppression filters (Flat, 75Hz Vocal, 150Hz Wind), 40ms fast unmute delay, ADC pre-amp calibration, talk/ambient mic gains, earpiece boost, and HD2 Hi-Fi mode.
* **Display Controls**: Sunlight High Brightness Mode (HBM 1100 nits) and AMOLED DC Dimming (Anti-Flicker).

### 🛡️ 4. Root & Stealth Engine
* Built-in **KernelSU-Next** (v1.0.5, UAPI v5) with manual hook mode.
* Integrated **SuSFS v2.3.0** with companion spoofer module for bootloader lock spoofing and Play Integrity bypass.

---

## 📦 Release Downloads (v1.6.1)

| Package | Description | Size | SHA-256 Checksum |
| :--- | :--- | :---: | :--- |
| **`AETHER-Kernel-mojito-v1.6.1-stable-mglru-ksunext-susfs.zip`** | Flashable AnyKernel3 Kernel Package | ~20 MB | `84abadcd76541b6677a0b2588b02a91b918243c74a0f472a2107c26f0e108cc5` |
| **`AETHER-Kernel-Control-WebUI-v1.6.1.zip`** | KernelSU Companion WebUI Module | ~103 KB | `5a1d293390e486b3358aeff478950fd9052d57ce4b186882773a1af40904f634` |
| **`AETHER-SuSFS-Stock-Spoofer-v2.3.0.zip`** | SuSFS Companion Spoofer Module | ~129 KB | `ab4cca25d4e1e976d5f59515f3d6808af659c731f45261c26db22d80b1211f74` |

---

## 📲 Installation Guide

### Prerequisites
1. Unlocked bootloader on Xiaomi Redmi Note 10 (`mojito` / `sunny`).
2. Custom Recovery (**TWRP** or **OrangeFox**).
3. Android 16 or Android 17 ROM.

### Flashing Steps
1. Reboot to recovery (TWRP / OrangeFox).
2. Flash `AETHER-Kernel-mojito-v1.6.1-stable-mglru-ksunext-susfs.zip`.
3. Reboot to System.
4. Open **KernelSU Manager** and install:
   - `AETHER-Kernel-Control-WebUI-v1.6.1.zip`
   - `AETHER-SuSFS-Stock-Spoofer-v2.3.0.zip`
5. Reboot to activate modules.

---

## 🎛️ Sysfs Nodes Reference (`/sys/kernel/aether/`)

| Node | Permissions | Description |
| :--- | :---: | :--- |
| `version` | RO | Subsystem version string |
| `engine_active` | RO | Returns `1` when ring-0 engine is running |
| `status` | RO | Unified kernel status telemetry |
| `charge_cutoff_limit` | RW | Cutoff target: `100` (Default), `50`-`95`, or `0` (Disabled) |
| `charge_cutoff_mode` | RW | `1` = Real Hardware Bypass, `0` = Standby |
| `instant_bypass` | RW | `1` = Force direct USB motherboard power at any % |
| `aacc_thermalguard` | RO | Live battery temp, voted FCC, and thermal pacing state |
| `charge_current_limit` | RW | Current limit in µA (`0` = Stock 33W Unlimited) |
| `mic_hpf_mode` | RW | `0` = Flat, `1` = 75Hz Vocal Clarity, `2` = 150Hz Noise Filter |
| `mic_unmute_delay` | RW | Unmute delay in ms (default `40`ms) |
| `hbm` | RW | Sunlight High Brightness Mode (`0` = Off, `1` = On) |
| `anti_flicker` | RW | AMOLED DC Dimming (`0` = Off, `1` = On) |
| `susfs_status` | RO | SuSFS v2.3.0 status |

---

## ⚖️ Credits & License

* Licensed under the **GNU General Public License v2 (GPL-2.0)**.
* **Credits**: AnyKernel3 (osm0sis), KernelSU-Next (backslashxx & tiann), SuSFS (simonpunk), BORE Scheduler (Masahito Suzuki & firelzrd), Google, Qualcomm, Xiaomi.
