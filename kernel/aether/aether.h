/* SPDX-License-Identifier: GPL-2.0 */
/*
 * AETHER Kernel Sandwiched Control Subsystem v1.6.1
 * Author: Kiran_Embedded
 *
 * Master Header: Subsystem Declarations & Core Interfaces
 *
 * Provides shared prototypes, exported state hooks, and sysfs visibility
 * helpers across modular engine components:
 *   - aether_core.c:    Root kobject /sys/kernel/aether, version & telemetry
 *   - aether_battery.c: Smart thermal pacing, 100% bypass hold, PMIC/TI voting
 *   - aether_audio.c:   Mic filters (HPF), ADC pre-amp, talk gain & HD2 Hi-Fi
 *   - aether_display.c: Sunlight High-Brightness-Mode (HBM) & DC Dimming
 *   - aether_perf.c:    TCP lowat pacing, dirty page pacing & sched idle
 */

#ifndef _AETHER_H_
#define _AETHER_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/types.h>

/* --- Sysfs Node Permissions Helper --- */
/*
 * Automatically grants read-write (0666) permissions to writable nodes
 * and read-only (0444) to status nodes so the WebUI service can interact
 * without needing root file-permission workarounds.
 */
umode_t aether_attr_is_visible(struct kobject *kobj, struct attribute *attr, int n);

/* --- Audio Subsystem States & Exports --- */
extern int aether_audio_bypass_mode;
extern int aether_mic_hpf_mode;
extern int aether_mic_unmute_delay;
extern int aether_mic_adc_gain;
extern int aether_mic_gain_bottom;
extern int aether_mic_gain_top;
extern int aether_earpiece_boost;
extern int aether_hifi_mode;

/* --- Battery & Power Subsystem States & Exports --- */
extern int aether_charge_current_limit;
bool aether_is_bypass_active(void);
void aether_notify_usb_change(void);

/* --- Display Subsystem DRM Hooks --- */
extern int aether_get_hbm(void);
extern int aether_set_hbm(int hbm_mode);
extern int aether_get_anti_flicker(void);
extern int aether_set_anti_flicker(int enable);
extern unsigned int aether_get_measured_fps(void);

/* --- Performance Subsystem States & Exports --- */
extern int aether_sched_idle_search;

/* --- Subsystem Initialization Prototypes --- */
int aether_battery_init(struct kobject *parent);
int aether_audio_init(struct kobject *parent);
int aether_display_init(struct kobject *parent);
int aether_perf_init(struct kobject *parent);

#endif /* _AETHER_H_ */
