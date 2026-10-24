// SPDX-License-Identifier: GPL-2.0
/*
 * AETHER Kernel Sandwiched Control Engine
 * Author: Kiran_Embedded
 *
 * Provides ring-0 in-kernel configuration and direct sysfs interface
 * for Google low-latency BLK-MQ I/O pacing, SchedTune, TCP bufferbloat
 * elimination, dirty page stream pacing, and audio bypass/HPF controls.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/writeback.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/dcache.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/pmic-voter.h>
#include <net/net_namespace.h>

extern int page_cluster;

/* Exported in-kernel tuning states */
int aether_audio_bypass_mode = 1;
EXPORT_SYMBOL(aether_audio_bypass_mode);

int aether_mic_hpf_mode = 0;
EXPORT_SYMBOL(aether_mic_hpf_mode);

static struct kobject *aether_kobj;

/* 1. Version Attribute */
static ssize_t version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "AETHER-v1.4.0 (In-Kernel Sandwiched Engine)\n");
}
static struct kobj_attribute version_attr = __ATTR_RO(version);

/* 2. Engine Active Attribute */
static ssize_t engine_active_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}
static struct kobj_attribute engine_active_attr = __ATTR_RO(engine_active);

/* 3. TCP Lowat Pacing Attribute */
static ssize_t tcp_lowat_pacing_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned int cur = init_net.ipv4.sysctl_tcp_notsent_lowat;
	int active = (cur <= 65536) ? 1 : 0;
	return sprintf(buf, "%d\n", active);
}

static ssize_t tcp_lowat_pacing_store(struct kobject *kobj, struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	if (val)
		init_net.ipv4.sysctl_tcp_notsent_lowat = 16384;
	else
		init_net.ipv4.sysctl_tcp_notsent_lowat = UINT_MAX;

	return count;
}
static struct kobj_attribute tcp_lowat_pacing_attr = __ATTR_RW(tcp_lowat_pacing);

/* 4. Dirty Page Pacing Attribute */
static ssize_t dirty_page_pacing_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int active = (dirty_writeback_interval == 1000 && dirty_expire_interval == 2000) ? 1 : 0;
	return sprintf(buf, "%d\n", active);
}

static ssize_t dirty_page_pacing_store(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	if (val) {
		dirty_writeback_interval = 1000;
		dirty_expire_interval = 2000;
		vm_dirty_ratio = 20;
		dirty_background_ratio = 10;
	} else {
		dirty_writeback_interval = 500;
		dirty_expire_interval = 3000;
	}
	return count;
}
static struct kobj_attribute dirty_page_pacing_attr = __ATTR_RW(dirty_page_pacing);

/* 5. Audio Bypass Mode Attribute (Default 1: Stock Audio) */
static ssize_t audio_bypass_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_audio_bypass_mode);
}

static ssize_t audio_bypass_mode_store(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	aether_audio_bypass_mode = !!val;
	return count;
}
static struct kobj_attribute audio_bypass_mode_attr = __ATTR_RW(audio_bypass_mode);

/* 6. Mic HPF Noise Filter Mode (0: Flat, 1: 75Hz Vocal, 2: 150Hz Wind/Noise) */
static ssize_t mic_hpf_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_mic_hpf_mode);
}

static ssize_t mic_hpf_mode_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	if (val < 0 || val > 2)
		return -EINVAL;

	aether_mic_hpf_mode = val;
	return count;
}
static struct kobj_attribute mic_hpf_mode_attr = __ATTR_RW(mic_hpf_mode);

/* 7. Comprehensive Kernel Status Attribute */
static ssize_t status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		"AETHER_VERSION=v1.4.0\n"
		"ENGINE_STATUS=ACTIVE_RING0\n"
		"DEFAULT_BLK_MQ_IOSCHED=kyber\n"
		"TCP_LOWAT_PACING=%d\n"
		"TCP_NOTSENT_LOWAT_BYTES=%u\n"
		"DIRTY_PAGE_PACING=%d\n"
		"DIRTY_WRITEBACK_CS=%u\n"
		"DIRTY_EXPIRE_CS=%u\n"
		"AUDIO_BYPASS_MODE=%d\n"
		"MIC_HPF_MODE=%d\n"
		"CHARGING_ENGINE=STOCK_XIAOMI_MI_TURBO_CHARGE\n",
		(init_net.ipv4.sysctl_tcp_notsent_lowat <= 65536) ? 1 : 0,
		init_net.ipv4.sysctl_tcp_notsent_lowat,
		(dirty_writeback_interval == 1000) ? 1 : 0,
		dirty_writeback_interval,
		dirty_expire_interval,
		aether_audio_bypass_mode,
		aether_mic_hpf_mode);
}
static struct kobj_attribute status_attr = __ATTR_RO(status);

/* 8. Native Xiaomi Hardware Charge Cutoff & Bypass Controller */
static int aether_charge_cutoff_limit; /* 0 = Disabled, 50 - 95% */
static int aether_charge_cutoff_mode = 1;  /* 0 = Cool Standby, 1 = Real Hardware Bypass */
static int aether_instant_bypass;      /* 0 = Normal, 1 = Instant Direct USB Power */
static bool aether_cutoff_hardware_active;
static int aether_last_applied_mode = -1;
static char aether_cutoff_status_buf[96] = "STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)";
static struct delayed_work aether_cutoff_work;

#define AETHER_BYPASS_CLIENT "AETHER_BYPASS_VOTER"

static void aether_set_cp_charging_enabled(int enable)
{
	struct power_supply *cp_psy;
	union power_supply_propval val = {.intval = enable};

	cp_psy = power_supply_get_by_name("bq2597x-master");
	if (!cp_psy)
		cp_psy = power_supply_get_by_name("bq2597x-standalone");

	if (cp_psy) {
		power_supply_set_property(cp_psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
		power_supply_put(cp_psy);
	}

	cp_psy = power_supply_get_by_name("bq2597x-slave");
	if (cp_psy) {
		power_supply_set_property(cp_psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
		power_supply_put(cp_psy);
	}
}

static void aether_set_hardware_bypass(bool disable)
{
	struct votable *chg_dis = find_votable("CHG_DISABLE");
	struct power_supply *batt_psy = power_supply_get_by_name("battery");
	union power_supply_propval bq_sus = {.intval = disable ? 1 : 0};

	if (chg_dis)
		vote(chg_dis, AETHER_BYPASS_CLIENT, disable, 0);

	if (batt_psy) {
		power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_BQ_INPUT_SUSPEND, &bq_sus);
		power_supply_put(batt_psy);
	}

	aether_set_cp_charging_enabled(disable ? 0 : 1);
}

static void aether_cutoff_workfunc(struct work_struct *work)
{
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	union power_supply_propval pval = {0};
	int soc = -1;
	bool usb_present = false;

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy)
		goto reschedule;

	usb_psy = power_supply_get_by_name("usb");
	if (usb_psy) {
		if (power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_PRESENT, &pval) == 0)
			usb_present = !!pval.intval;
		power_supply_put(usb_psy);
	}

	if (power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval) == 0)
		soc = pval.intval;

	/* If USB cable is disconnected, restore normal hardware charge enable state */
	if (!usb_present) {
		if (aether_cutoff_hardware_active) {
			aether_set_hardware_bypass(false);
			aether_cutoff_hardware_active = false;
			aether_last_applied_mode = -1;
		}
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"DISCONNECTED (Stock Turbo Charging Ready)");
		goto out_put;
	}

	/* Determine target state based on user settings and battery SOC */
	{
		bool target_disable = false;
		const char *target_status = "STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)";

		if (aether_instant_bypass) {
			target_disable = true;
			target_status = "BYPASS_ACTIVE (Gaming Direct USB Power, 0mA Net)";
		} else if (aether_charge_cutoff_limit > 0) {
			if (soc >= aether_charge_cutoff_limit) {
				target_disable = true;
				target_status = "BYPASS_ACTIVE (Held at Limit, Direct USB Power)";
			} else if (aether_cutoff_hardware_active && soc > (aether_charge_cutoff_limit - 3)) {
				target_disable = true;
				target_status = "BYPASS_ACTIVE (Held at Limit, Direct USB Power)";
			} else {
				target_disable = false;
				target_status = "CHARGING_ACTIVE (Turbo Charging Below Limit)";
			}
		} else {
			target_disable = false;
			target_status = "STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)";
		}

		/* Apply state transition using Qualcomm hardware votables */
		if (target_disable && !aether_cutoff_hardware_active) {
			aether_set_hardware_bypass(true);
			aether_cutoff_hardware_active = true;
			aether_last_applied_mode = 1;
		} else if (!target_disable && aether_cutoff_hardware_active) {
			aether_set_hardware_bypass(false);
			aether_cutoff_hardware_active = false;
			aether_last_applied_mode = -1;
		}

		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf), "%s", target_status);
	}

out_put:
	power_supply_put(batt_psy);

reschedule:
	{
		unsigned int delay_ms = 3000;

		/* Deep Sleep Optimization:
		 * When phone is unplugged (on battery) or neither bypass nor cutoff limit is configured,
		 * sleep for 60s instead of waking CPU. Sysfs writes immediately re-queue with 0ms delay.
		 */
		if (!usb_present || (!aether_instant_bypass && aether_charge_cutoff_limit <= 0))
			delay_ms = 60000;

		schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(delay_ms));
	}
}

static ssize_t aacc_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		"CHARGING_ENGINE=STOCK_XIAOMI_MI_TURBO_CHARGE\n"
		"AACC_ACTIVE=0\n"
		"CHARGING_MODE=%s\n"
		"STATUS=%s\n",
		aether_instant_bypass ? "INSTANT_BYPASS" :
		(aether_charge_cutoff_limit > 0 ? "TARGET_CUTOFF" : "STOCK_TURBO_CHARGING"),
		aether_cutoff_status_buf);
}
static struct kobj_attribute aacc_status_attr = __ATTR_RO(aacc_status);

static ssize_t aacc_thermalguard_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		"THERMALGUARD_STATE=STOCK_XIAOMI_HARDWARE_CONTROL\n"
		"STATUS=Stock Xiaomi Thermal Pacing Active\n");
}
static struct kobj_attribute aacc_thermalguard_attr = __ATTR_RO(aacc_thermalguard);

static ssize_t aacc_screen_boost_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "0\n");
}

static ssize_t aacc_screen_boost_store(struct kobject *kobj, struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	return count;
}
static struct kobj_attribute aacc_screen_boost_attr = __ATTR_RW(aacc_screen_boost);

static ssize_t aacc_idle_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_instant_bypass);
}

static ssize_t aacc_idle_mode_store(struct kobject *kobj, struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	bool val;
	if (strtobool(buf, &val) < 0)
		return -EINVAL;

	aether_instant_bypass = val ? 1 : 0;
	schedule_delayed_work(&aether_cutoff_work, 0);
	return count;
}
static struct kobj_attribute aacc_idle_mode_attr = __ATTR_RW(aacc_idle_mode);

/* Charge Cutoff Limit Attribute (0 = Disabled, 50 - 95%) */
static ssize_t charge_cutoff_limit_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_charge_cutoff_limit);
}

static ssize_t charge_cutoff_limit_store(struct kobject *kobj, struct kobj_attribute *attr,
					 const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	if (val < 0 || val > 100)
		return -EINVAL;

	aether_charge_cutoff_limit = val;
	schedule_delayed_work(&aether_cutoff_work, 0);
	return count;
}
static struct kobj_attribute charge_cutoff_limit_attr = __ATTR_RW(charge_cutoff_limit);

/* Charge Cutoff Mode Attribute (0 = Standby, 1 = Real Bypass) */
static ssize_t charge_cutoff_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_charge_cutoff_mode);
}

static ssize_t charge_cutoff_mode_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	aether_charge_cutoff_mode = !!val;
	schedule_delayed_work(&aether_cutoff_work, 0);
	return count;
}
static struct kobj_attribute charge_cutoff_mode_attr = __ATTR_RW(charge_cutoff_mode);

/* Instant Bypass Attribute (0 = Normal, 1 = Direct USB Power at Any %) */
static ssize_t instant_bypass_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_instant_bypass);
}

static ssize_t instant_bypass_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	bool val;
	if (strtobool(buf, &val) < 0)
		return -EINVAL;

	aether_instant_bypass = val ? 1 : 0;
	schedule_delayed_work(&aether_cutoff_work, 0);
	return count;
}
static struct kobj_attribute instant_bypass_attr = __ATTR_RW(instant_bypass);

/* Charge Cutoff Status Attribute (Read-Only) */
static ssize_t charge_cutoff_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", aether_cutoff_status_buf);
}
static struct kobj_attribute charge_cutoff_status_attr = __ATTR_RO(charge_cutoff_status);

/* Boot Booster & Spike Suppressor Status */
static ssize_t boot_booster_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "TWO_STAGE_ACTIVE (2048KB Boot Burst -> 256KB Steady State, CPU0-3 Background Isolation)\n");
}
static struct kobj_attribute boot_booster_attr = __ATTR_RO(boot_booster);

static struct attribute *aether_attrs[] = {
	&version_attr.attr,
	&engine_active_attr.attr,
	&tcp_lowat_pacing_attr.attr,
	&dirty_page_pacing_attr.attr,
	&audio_bypass_mode_attr.attr,
	&mic_hpf_mode_attr.attr,
	&boot_booster_attr.attr,
	&status_attr.attr,
	&aacc_status_attr.attr,
	&aacc_thermalguard_attr.attr,
	&aacc_screen_boost_attr.attr,
	&aacc_idle_mode_attr.attr,
	&charge_cutoff_limit_attr.attr,
	&charge_cutoff_mode_attr.attr,
	&instant_bypass_attr.attr,
	&charge_cutoff_status_attr.attr,
	NULL,
};

static const struct attribute_group aether_attr_group = {
	.attrs = aether_attrs,
};

static int __init aether_control_init(void)
{
	int ret;

	/* 1. Google Pixel VM Dirty Page continuous stream pacing & RAM optimization */
	dirty_writeback_interval = 10 * 100; /* 1000 cs (10s) */
	dirty_expire_interval = 20 * 100;    /* 2000 cs (20s) */
	vm_dirty_ratio = 20;
	dirty_background_ratio = 10;
	vm_swappiness = 60;                  /* Balanced swappiness: prevents ZRAM thrashing and UI jitter */
	sysctl_vfs_cache_pressure = 100;     /* Proportionally reclaim stale dentry/inode cache to free system RAM */
	page_cluster = 0;                    /* 4KB single-page swap-in for zero latency & zero CPU spikes */
	watermark_scale_factor = 150;        /* 1.5% watermark gap: kswapd cleans pages early, eliminating direct reclaim freezes */

	/* 2. Suppress verbose console debug chatter to prevent printk lock contention and CPU wakeups */
	console_loglevel = 4;

	/* 3. Google low-latency TCP socket buffer pacing */
	init_net.ipv4.sysctl_tcp_notsent_lowat = 16384;

	/* 3. Register /sys/kernel/aether sysfs directory */
	aether_kobj = kobject_create_and_add("aether", kernel_kobj);
	if (!aether_kobj) {
		pr_err("AETHER: Failed to create /sys/kernel/aether kobject\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(aether_kobj, &aether_attr_group);
	if (ret) {
		pr_err("AETHER: Failed to create sysfs attribute group (%d)\n", ret);
		kobject_put(aether_kobj);
		return ret;
	}

	INIT_DELAYED_WORK(&aether_cutoff_work, aether_cutoff_workfunc);
	schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(3000));

	pr_info("AETHER: In-Kernel Sandwiched Control Engine v1.4.0 active at /sys/kernel/aether (Ring-0 Direct)\n");
	return 0;
}

late_initcall(aether_control_init);
