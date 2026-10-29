# AETHER Kernel v1.8 — Redmi Note 10 (mojito / sunny)

- **Device:** Xiaomi Redmi Note 10 (`mojito` / `sunny`, Qualcomm Snapdragon 678 / SM6150)
- **Tested Versions:** Android 16 & Android 17 (Tested & verified)
- **Untested Versions:** Android 11, 12, 13, 14, 15 (We did not test on Android 11–15; flash at your own risk)
- **Root Support:** KernelSU-Next (v1.0.5) & SuSFS (v2.3.0)

---

## What's New in v1.8

### 1. New Floating Real-Time FPS Meter
- **Hardware-Level Frame Tracking:** Brand new floating FPS meter overlay reading directly from display hardware.
- **No Artificial Caps:** Displays true refresh rates and frame drops without forced 60 FPS capping.
- **Fixed QS Toggle:** Quick Settings tile now turns the floating meter on and off reliably with a single tap.
- **Battery Efficient:** Automatically pauses background frame polling when the screen turns off.
- **Minimal RAM Impact:** Consumes ~4 MB when visible, and 0 MB idle RAM when toggled off.

### 2. Enhanced & Redesigned WebUI Interface
- **Redesigned Interface:** Modern layout with clear categorized cards, responsive pill selectors, and clean controls.
- **Instant Hardware Updates:** Direct hardware sysfs writes with real-time toast confirmations.
- **Safety Revert Timer:** 5-second countdown bar to easily undo changes if needed.
- **Persistent Settings:** Your chosen settings automatically restore after every reboot.

### 3. Upgraded Charging & Battery Care System
- **Hardware Bypass Charging:** Power the phone directly from the charger with 0 mA battery current — prevents battery heat and degradation during gaming sessions.
- **Screen-On Fast Charging Boost:** Prevents aggressive throttling when using the phone while plugged in, allowing steady fast charging even with the display turned on.
- **Custom Battery Cutoff Limits:** Set charging limits (e.g., 80%, 85%, 90%, 95%, 100%) in WebUI. The phone charges to the target level, then holds safely in USB bypass mode.
- **Cooler Fast Charging:** Optimized thermal thresholds to keep battery temperature safe while maintaining steady charge speed.
- **Reboot Persistence:** Charging limits and bypass preferences persist cleanly across reboots.

### 4. Standalone Quick Settings Panel Support
- Works independently — **no root, KernelSU, Magisk, or WebUI module required**.
- Notification drawer quick toggles:
  - **Bypass Charging:** Powers the device directly from the charger to stop battery heat during gaming.
  - **Sunlight Mode (HBM):** Boosts screen brightness outdoors up to 1100 nits.
  - **AMOLED DC Dimming:** Eliminates low-brightness screen flicker and eye strain.
  - **Touch Boost:** Increases touch sampling rate for responsive gaming.
  - **Performance Modes:** Quickly toggle between Battery, Balanced, and Performance.
  - **FPS Meter:** Instant floating frame counter toggle.
- Automatically installed as a system app when flashing kernel, or can be installed as standalone APKs.

### 5. RAM Optimizations & Zero Background Daemons
- **Removed Background Daemon:** Deleted the legacy `aetherd` daemon, freeing active RAM.
- **Multi-Gen LRU (MGLRU) & ZRAM:** Optimized memory reclaim and compression so multitasking stays smooth without aggressive app kills.
- **Improved Deep Sleep:** Reduced battery drain when screen is off.

### 6. Android 17 & 16 Tested & Fixed
- **Android 17 Fixes:** Enabled `sys.use_memfd` automatically to prevent boot issues on Android 17.
- **Camera & Sensors:** Fixed camera preview, auto-rotation, ambient brightness, and screen recording on Android 17 ROMs.
- **Android 16 Verified:** Clean default behavior with no conflicting overrides.
- *Note:* Android 11 through 15 were not tested in this release.

### 7. Clean Source Code
- Cleaned up source code, removed unnecessary debug logs, dead code, and test hooks.

---

## Downloads

| Package | Description | SHA-256 Checksum |
| :--- | :--- | :--- |
| **Kernel ZIP** | `AETHER-Kernel-mojito-v1.8-stable-mglru-ksunext-susfs.zip` | `b39828f6112507900c8f21796dcc38f30a16ade8419eb6e4828ee3cdfc468ee7` |
| **WebUI Module** | `AETHER-Kernel-Control-WebUI-v1.8.zip` | `b5f86ca03cb3496413aa07896e356b482519e32670f8223f68304fa2930f249c` |
| **SuSFS Spoofer** | `AETHER-SuSFS-Stock-Spoofer-v2.3.0.zip` | `ab4cca25d4e1e976d5f59515f3d6808af659c731f45261c26db22d80b1211f74` |

---

## Installation Steps

1. Boot into custom recovery (OrangeFox or TWRP).
2. Flash `AETHER-Kernel-mojito-v1.8-stable-mglru-ksunext-susfs.zip`.
3. Reboot to system.
4. *(Optional)* Flash `AETHER-Kernel-Control-WebUI-v1.8.zip` via KernelSU-Next or Magisk for full dashboard control.
