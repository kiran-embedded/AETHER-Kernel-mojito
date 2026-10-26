// SPDX-License-Identifier: GPL-2.0
/*
 * AETHER Kernel Sandwiched Control Subsystem v1.6.1
 * Author: Kiran_Embedded
 *
 * Module: Core Engine & Master Sysfs Root (/sys/kernel/aether)
 *
 * Responsibilities:
 *  - Spawns the central /sys/kernel/aether kobject under /sys/kernel/
 *  - Dispatches modular initialization to battery, audio, display, and perf
 *  - Exposes master kernel version, engine health, and unified status telemetry
 *  - Enforces uniform 0666/0444 node visibility across all subsystem groups
 */

#include "aether.h"
#include <linux/writeback.h>
#include <net/net_namespace.h>

static struct kobject *aether_kobj;

/*
 * aether_attr_is_visible:
 * Uniform sysfs permissions filter.
 * Ensures writable nodes are accessible as 0666 (rw-rw-rw-) and read-only
 * telemetry attributes as 0444 (r--r--r--), matching WebUI service requirements.
 */
umode_t aether_attr_is_visible(struct kobject *kobj, struct attribute *attr, int n)
{
	umode_t mode = attr->mode;

	if (mode & S_IWUSR)
		mode |= 0666;
	else
		mode |= 0444;

	return mode;
}

/* 1. Subsystem Version Attribute */
static ssize_t version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "AETHER-v1.6.1 (In-Kernel Sandwiched Engine)\n");
}
static struct kobj_attribute version_attr = __ATTR_RO(version);

/* 2. Ring-0 Engine Active Attribute */
static ssize_t engine_active_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}
static struct kobj_attribute engine_active_attr = __ATTR_RO(engine_active);

/* 3. Comprehensive Subsystem Status Attribute */
static ssize_t status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		"AETHER_VERSION=v1.6.1\n"
		"ENGINE_STATUS=ACTIVE_RING0\n"
		"DEFAULT_BLK_MQ_IOSCHED=deadline\n"
		"TCP_LOWAT_PACING=%d\n"
		"TCP_NOTSENT_LOWAT_BYTES=%u\n"
		"DIRTY_PAGE_PACING=%d\n"
		"DIRTY_WRITEBACK_CS=%u\n"
		"DIRTY_EXPIRE_CS=%u\n"
		"AUDIO_BYPASS_MODE=%d\n"
		"MIC_HPF_MODE=%d\n"
		"MIC_UNMUTE_DELAY=%d\n"
		"MIC_ADC_GAIN=%d\n"
		"MIC_GAIN_BOTTOM=%d\n"
		"MIC_GAIN_TOP=%d\n"
		"EARPIECE_BOOST=%d\n"
		"HIFI_MODE=%d\n"
		"CHARGE_CURRENT_LIMIT=%d\n"
		"CHARGING_ENGINE=STOCK_XIAOMI_MI_TURBO_CHARGE\n",
		(init_net.ipv4.sysctl_tcp_notsent_lowat <= 65536) ? 1 : 0,
		init_net.ipv4.sysctl_tcp_notsent_lowat,
		(dirty_writeback_interval == 1000) ? 1 : 0,
		dirty_writeback_interval,
		dirty_expire_interval,
		aether_audio_bypass_mode,
		aether_mic_hpf_mode,
		aether_mic_unmute_delay,
		aether_mic_adc_gain,
		aether_mic_gain_bottom,
		aether_mic_gain_top,
		aether_earpiece_boost,
		aether_hifi_mode,
		aether_charge_current_limit);
}
static struct kobj_attribute status_attr = __ATTR_RO(status);

/* 4. Boot Booster & Spike Suppressor Status */
static ssize_t boot_booster_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "TWO_STAGE_ACTIVE (1024KB Boot Warm-Start -> 384KB Steady State, CPU0-3 Background Isolation)\n");
}
static struct kobj_attribute boot_booster_attr = __ATTR_RO(boot_booster);

/* 5. SuSFS Kernel Stealth Subsystem Status */
static ssize_t susfs_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
#ifdef CONFIG_KSU_SUSFS
	return sprintf(buf, "v2.3.0 Active\n");
#else
	return sprintf(buf, "Inactive\n");
#endif
}
static struct kobj_attribute susfs_status_attr = __ATTR_RO(susfs_status);

static struct attribute *aether_core_attrs[] = {
	&version_attr.attr,
	&engine_active_attr.attr,
	&status_attr.attr,
	&boot_booster_attr.attr,
	&susfs_status_attr.attr,
	NULL,
};

static const struct attribute_group aether_core_attr_group = {
	.is_visible = aether_attr_is_visible,
	.attrs = aether_core_attrs,
};

/*
 * aether_core_init:
 * Master late-init entrypoint for the AETHER in-kernel subsystem.
 * Creates the master /sys/kernel/aether kobject and initializes all modules.
 */
static int __init aether_core_init(void)
{
	int ret;

	/* Create root /sys/kernel/aether sysfs directory */
	aether_kobj = kobject_create_and_add("aether", kernel_kobj);
	if (!aether_kobj) {
		pr_err("AETHER: Failed to create /sys/kernel/aether kobject\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(aether_kobj, &aether_core_attr_group);
	if (ret) {
		pr_err("AETHER: Failed to register core attributes (%d)\n", ret);
		kobject_put(aether_kobj);
		return ret;
	}

	/* Initialize modular subsystems */
	aether_perf_init(aether_kobj);
	aether_audio_init(aether_kobj);
	aether_display_init(aether_kobj);
	aether_battery_init(aether_kobj);

	pr_info("AETHER: Core Sandwiched Subsystem initialized at /sys/kernel/aether\n");
	return 0;
}

late_initcall(aether_core_init);
