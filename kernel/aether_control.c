// SPDX-License-Identifier: GPL-2.0
/*
 * AETHER Kernel Sandwiched Control Engine v1.6.0
 * Author: Kiran_Embedded
 *
 * Provides ring-0 in-kernel configuration and direct sysfs interface
 * for Google low-latency BLK-MQ I/O pacing, SchedTune, TCP bufferbloat
 * elimination, dirty page stream pacing, audio bypass/HPF controls,
 * hardware battery charge cutoff, instant bypass charging, Sunlight HBM,
 * and AMOLED anti-flicker (DC dimming) without overriding dev-set values.
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

/* Exported in-kernel tuning states */
int aether_audio_bypass_mode = 1;
EXPORT_SYMBOL(aether_audio_bypass_mode);

int aether_mic_hpf_mode = 1;
EXPORT_SYMBOL(aether_mic_hpf_mode);

/* Display Hooks */
extern int aether_get_hbm(void);
extern int aether_set_hbm(int hbm_mode);
extern int aether_get_anti_flicker(void);
extern int aether_set_anti_flicker(int enable);

static struct kobject *aether_kobj;

/* 1. Version Attribute */
static ssize_t version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "AETHER-v1.6.0 (In-Kernel Sandwiched Engine)\n");
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
	} else {
		dirty_writeback_interval = 500;
		dirty_expire_interval = 1500;
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

/* 7. Mic Unmute Delay Attribute (in milliseconds, default 40ms) */
static int aether_mic_unmute_delay = 40;
static ssize_t mic_unmute_delay_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_mic_unmute_delay);
}

static ssize_t mic_unmute_delay_store(struct kobject *kobj, struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;
	if (val < 5 || val > 200)
		return -EINVAL;
	aether_mic_unmute_delay = val;
	return count;
}
static struct kobj_attribute mic_unmute_delay_attr = __ATTR_RW(mic_unmute_delay);

/* 8. Mic ADC Pre-Amp Gain (8 - 15, default 12) */
static int aether_mic_adc_gain = 12;
static ssize_t mic_adc_gain_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_mic_adc_gain);
}

static ssize_t mic_adc_gain_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;
	if (val < 8 || val > 15)
		return -EINVAL;
	aether_mic_adc_gain = val;
	return count;
}
static struct kobj_attribute mic_adc_gain_attr = __ATTR_RW(mic_adc_gain);

/* 9. Mic Bottom / Primary Talk Gain dB (-12 to +12, default 0) */
static int aether_mic_gain_bottom = 0;
static ssize_t mic_gain_bottom_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_mic_gain_bottom);
}

static ssize_t mic_gain_bottom_store(struct kobject *kobj, struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;
	if (val < -12 || val > 12)
		return -EINVAL;
	aether_mic_gain_bottom = val;
	return count;
}
static struct kobj_attribute mic_gain_bottom_attr = __ATTR_RW(mic_gain_bottom);

/* 10. Mic Top / Ambient Reference Gain dB (-12 to +12, default 0) */
static int aether_mic_gain_top = 0;
static ssize_t mic_gain_top_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_mic_gain_top);
}

static ssize_t mic_gain_top_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;
	if (val < -12 || val > 12)
		return -EINVAL;
	aether_mic_gain_top = val;
	return count;
}
static struct kobj_attribute mic_gain_top_attr = __ATTR_RW(mic_gain_top);

/* 11. Receiver Earpiece Volume Boost dB (0 to 6, default 0) */
static int aether_earpiece_boost = 0;
static ssize_t earpiece_boost_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_earpiece_boost);
}

static ssize_t earpiece_boost_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;
	if (val < 0 || val > 6)
		return -EINVAL;
	aether_earpiece_boost = val;
	return count;
}
static struct kobj_attribute earpiece_boost_attr = __ATTR_RW(earpiece_boost);

/* 12. Headphone Hi-Fi High-Performance Mode (0: Normal, 1: HD2 Hi-Fi) */
static int aether_hifi_mode = 0;
static ssize_t hifi_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_hifi_mode);
}

static ssize_t hifi_mode_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;
	aether_hifi_mode = !!val;
	return count;
}
static struct kobj_attribute hifi_mode_attr = __ATTR_RW(hifi_mode);

/* 13. Charging Current Limit (uA, 0 = Stock 33W Unlimited) */
static int aether_charge_current_limit = 0;
static ssize_t charge_current_limit_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_charge_current_limit);
}

static ssize_t charge_current_limit_store(struct kobject *kobj, struct kobj_attribute *attr,
					  const char *buf, size_t count)
{
	struct power_supply *batt_psy;
	union power_supply_propval val;
	int limit;

	if (kstrtoint(buf, 10, &limit) < 0)
		return -EINVAL;
	if (limit < 0)
		return -EINVAL;

	aether_charge_current_limit = limit;
	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		val.intval = limit > 0 ? limit : 6000000;
		power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &val);
		power_supply_changed(batt_psy);
		power_supply_put(batt_psy);
	}
	return count;
}
static struct kobj_attribute charge_current_limit_attr = __ATTR_RW(charge_current_limit);

/* 14. Comprehensive Kernel Status Attribute */
static ssize_t status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		"AETHER_VERSION=v1.6.0\n"
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

/* 8. Native Xiaomi PMIC charge-hold controller */
static int aether_charge_cutoff_limit; /* 0 = Disabled, 50 - 95% */
static int aether_charge_cutoff_mode = 1;  /* 0 = Cool Standby, 1 = Real Hardware Bypass */
static int aether_instant_bypass;      /* 0 = Normal, 1 = Instant Direct USB Power */
static bool aether_cutoff_hardware_active;
static char aether_cutoff_status_buf[96] = "STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)";
static struct delayed_work aether_cutoff_work;

#define AETHER_CHARGE_HOLD_CLIENT "AETHER_CHARGE_HOLD_VOTER"

bool aether_is_bypass_active(void)
{
	return READ_ONCE(aether_instant_bypass) ||
		READ_ONCE(aether_cutoff_hardware_active);
}
EXPORT_SYMBOL(aether_is_bypass_active);

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

static int aether_set_hardware_bypass(bool disable)
{
	struct votable *chg_dis = find_votable("CHG_DISABLE");
	struct power_supply *batt_psy = power_supply_get_by_name("battery");
	union power_supply_propval val = { .intval = disable ? 0 : 1 };
	int ret = 0;

	if (chg_dis)
		vote(chg_dis, AETHER_CHARGE_HOLD_CLIENT, disable, 0);

	if (batt_psy) {
		power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
		power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &val);
		power_supply_changed(batt_psy);
		power_supply_put(batt_psy);
	} else if (!chg_dis) {
		return -ENODEV;
	}

	aether_set_cp_charging_enabled(disable ? 0 : 1);

	return ret;
}

static bool aether_is_power_connected(void)
{
	struct power_supply *psy;
	union power_supply_propval pval = {0};
	bool connected = false;

	psy = power_supply_get_by_name("usb");
	if (psy) {
		if (power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &pval) == 0 && pval.intval)
			connected = true;
		if (!connected && power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &pval) == 0 && pval.intval)
			connected = true;
		power_supply_put(psy);
	}

	if (connected)
		return true;

	psy = power_supply_get_by_name("main");
	if (psy) {
		if (power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &pval) == 0 && pval.intval)
			connected = true;
		if (!connected && power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &pval) == 0 && pval.intval)
			connected = true;
		power_supply_put(psy);
	}

	if (connected)
		return true;

	psy = power_supply_get_by_name("battery");
	if (psy) {
		if (power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &pval) == 0) {
			if (pval.intval == POWER_SUPPLY_STATUS_CHARGING ||
			    pval.intval == POWER_SUPPLY_STATUS_FULL ||
			    pval.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
				connected = true;
		}
		power_supply_put(psy);
	}

	return connected;
}

static void aether_cutoff_workfunc(struct work_struct *work)
{
	struct power_supply *batt_psy;
	union power_supply_propval pval = {0};
	int soc = -1;
	bool power_connected = aether_is_power_connected();

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		if (power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval) == 0)
			soc = pval.intval;
		power_supply_put(batt_psy);
	}

	/* If phone is completely unplugged and user did NOT force instant bypass, restore charging */
	if (!power_connected && !aether_instant_bypass) {
		if (aether_cutoff_hardware_active) {
			aether_cutoff_hardware_active = false;
			aether_set_hardware_bypass(false);
		}
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"DISCONNECTED (Stock Turbo Charging Ready)");
		goto reschedule;
	}

	/* Determine target state based on user settings and battery SOC */
	{
		bool target_disable = false;
		const char *target_status = "STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)";

		if (aether_instant_bypass) {
			target_disable = true;
			target_status = "CHARGE_HOLD_ACTIVE (Instant mode; USB input available)";
		} else if (aether_charge_cutoff_limit > 0 && soc > 0) {
			if (soc >= aether_charge_cutoff_limit) {
				target_disable = true;
				target_status = "CHARGE_HOLD_ACTIVE (Target reached; USB input available)";
			} else if (aether_cutoff_hardware_active && soc > (aether_charge_cutoff_limit - 3)) {
				target_disable = true;
				target_status = "CHARGE_HOLD_ACTIVE (Hysteresis; USB input available)";
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
			aether_cutoff_hardware_active = true;
			if (aether_set_hardware_bypass(true)) {
				aether_cutoff_hardware_active = false;
				target_status = "CHARGE_HOLD_ERROR (charger votable unavailable)";
			}
		} else if (!target_disable && aether_cutoff_hardware_active) {
			aether_cutoff_hardware_active = false;
			if (aether_set_hardware_bypass(false))
				target_status = "CHARGE_RESTORE_ERROR (charger votable unavailable)";
		}

		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf), "%s", target_status);
	}

reschedule:
	{
		unsigned int delay_ms;

		/*
		 * Adaptive Deep Sleep Optimization:
		 * - No features active + unplugged: STOP entirely (0 CPU wakes, allows SoC PC3/AOSS)
		 * - Unplugged + cutoff configured: 60s (passive wait for charger reconnect)
		 * - Unplugged + instant bypass forced: 30s (bypass without cable is a no-op)
		 * - Plugged + no cutoff/bypass: 15s (lazy charger monitoring)
		 * - Bypass active: 10s (hardware votable holding steady)
		 * - Plugged + approaching cutoff threshold: 5s (precise SOC cutoff)
		 */
		if (!power_connected && !aether_instant_bypass && aether_charge_cutoff_limit <= 0) {
			/* Nothing to do — stop workqueue entirely to allow full deep sleep */
			return;
		} else if (!power_connected && !aether_instant_bypass) {
			delay_ms = 60000; /* 60s: passive wait for cable */
		} else if (!power_connected && aether_instant_bypass) {
			delay_ms = 30000; /* 30s: bypass without cable */
		} else if (aether_charge_cutoff_limit <= 0 && !aether_instant_bypass) {
			delay_ms = 15000; /* 15s: plugged but no cutoff/bypass */
		} else if (aether_cutoff_hardware_active) {
			delay_ms = 10000; /* 10s: bypass holding steady */
		} else {
			delay_ms = 5000; /* 5s: actively approaching cutoff threshold */
		}

		schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(delay_ms));
	}
}

static ssize_t aacc_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		"CHARGING_ENGINE=STOCK_XIAOMI_MI_TURBO_CHARGE\n"
		"AACC_ACTIVE=%d\n"
		"CHARGING_MODE=%s\n"
		"STATUS=%s\n",
		aether_is_bypass_active() ? 1 : 0,
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
	if (aether_instant_bypass) {
		aether_cutoff_hardware_active = true;
		aether_set_hardware_bypass(true);
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"CHARGE_HOLD_ACTIVE (Instant mode; USB input available)");
	} else {
		aether_cutoff_hardware_active = false;
		aether_set_hardware_bypass(false);
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)");
	}
	cancel_delayed_work(&aether_cutoff_work);
	schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(1000));
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

	if (val != 0 && (val < 50 || val > 95))
		return -EINVAL;

	aether_charge_cutoff_limit = val;
	cancel_delayed_work(&aether_cutoff_work);
	schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(1000));
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
	cancel_delayed_work(&aether_cutoff_work);
	schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(1000));
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
	if (aether_instant_bypass) {
		aether_cutoff_hardware_active = true;
		aether_set_hardware_bypass(true);
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"CHARGE_HOLD_ACTIVE (Instant mode; USB input available)");
	} else {
		aether_cutoff_hardware_active = false;
		aether_set_hardware_bypass(false);
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)");
	}
	cancel_delayed_work(&aether_cutoff_work);
	schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(1000));
	return count;
}
static struct kobj_attribute instant_bypass_attr = __ATTR_RW(instant_bypass);

/* Charge Cutoff Status Attribute (Read-Only) */
static ssize_t charge_cutoff_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", aether_cutoff_status_buf);
}
static struct kobj_attribute charge_cutoff_status_attr = __ATTR_RO(charge_cutoff_status);

/* Sched Idle Search (Top-App Prefer Idle Pacing) */
static int aether_sched_idle_search;

static ssize_t sched_idle_search_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_sched_idle_search);
}

static ssize_t sched_idle_search_store(struct kobject *kobj, struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	bool val;
	if (strtobool(buf, &val) < 0)
		return -EINVAL;

	aether_sched_idle_search = val ? 1 : 0;
	return count;
}
static struct kobj_attribute sched_idle_search_attr = __ATTR_RW(sched_idle_search);

/* Sunlight HBM (High Brightness Mode 1100 Nits) */
static ssize_t hbm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_get_hbm());
}

static ssize_t hbm_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	val = aether_set_hbm(val);
	return val ? val : count;
}
static struct kobj_attribute hbm_attr = __ATTR_RW(hbm);

/* AMOLED Anti-Flicker / DC Dimming */
static ssize_t anti_flicker_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_get_anti_flicker());
}

static ssize_t anti_flicker_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;

	val = aether_set_anti_flicker(val);
	return val ? val : count;
}
static struct kobj_attribute anti_flicker_attr = __ATTR_RW(anti_flicker);

/* Boot Booster & Spike Suppressor Status */
static ssize_t boot_booster_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "TWO_STAGE_ACTIVE (1024KB Boot Warm-Start -> 384KB Steady State, CPU0-3 Background Isolation)\n");
}
static struct kobj_attribute boot_booster_attr = __ATTR_RO(boot_booster);

static struct attribute *aether_attrs[] = {
	&version_attr.attr,
	&engine_active_attr.attr,
	&tcp_lowat_pacing_attr.attr,
	&dirty_page_pacing_attr.attr,
	&audio_bypass_mode_attr.attr,
	&mic_hpf_mode_attr.attr,
	&mic_unmute_delay_attr.attr,
	&mic_adc_gain_attr.attr,
	&mic_gain_bottom_attr.attr,
	&mic_gain_top_attr.attr,
	&earpiece_boost_attr.attr,
	&hifi_mode_attr.attr,
	&charge_current_limit_attr.attr,
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
	&sched_idle_search_attr.attr,
	&hbm_attr.attr,
	&anti_flicker_attr.attr,
	NULL,
};

static umode_t aether_is_visible(struct kobject *kobj, struct attribute *attr, int n)
{
	umode_t mode = attr->mode;
	if (mode & S_IWUSR)
		mode |= 0666;
	else
		mode |= 0444;
	return mode;
}

static const struct attribute_group aether_attr_group = {
	.is_visible = aether_is_visible,
	.attrs = aether_attrs,
};

/* External in-kernel MM hooks kept clean */

static int __init aether_control_init(void)
{
	int ret;

	/*
	 * Pure Ring-0 Control Engine:
	 * We intentionally DO NOT overwrite developer-calibrated VM / scheduler
	 * tunings here. The kernel developer's pre-configured settings (e.g. BORE,
	 * MGLRU, schedutil rate limits, dirty ratios) remain fully respected.
	 */

	/* Create /sys/kernel/aether sysfs directory */
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

	/* In-kernel network low-latency and bufferbloat pacing defaults */
	init_net.ipv4.sysctl_tcp_fastopen = 3;
	init_net.ipv4.sysctl_tcp_notsent_lowat = 16384;

	INIT_DELAYED_WORK(&aether_cutoff_work, aether_cutoff_workfunc);
	schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(3000));

	pr_info("AETHER: In-Kernel Control Engine active at /sys/kernel/aether\n");
	return 0;
}

late_initcall(aether_control_init);
