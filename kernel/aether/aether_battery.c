// SPDX-License-Identifier: GPL-2.0
/*
 * AETHER Kernel Sandwiched Control Subsystem v1.6.1
 * Author: Kiran_Embedded
 *
 * Module: Smart Battery, Predictive PD-MSCC Thermal Pacing & Hardware Bypass Engine
 * Target: Xiaomi Redmi Note 10 (mojito / sunny) - Qualcomm SM6150 (Snapdragon 678)
 *
 * Architecture & Hardware Control:
 *  - Qualcomm SMB5 PMIC + TI BQ2597x Switched-Capacitor Dual Charge Pump
 *  - Predictive Proportional-Derivative (PD) Thermal Engine (dT/dt velocity lead)
 *  - Dynamic Multi-Stage Constant Current (MSCC) Impedance-Matched Taper
 *  - TI BQ2597x 2:1 Sweet-Spot Hold & SMB5 Buck Hot-Zone Avoidance (Cool Motherboard)
 *  - Optimized Thermal Knots (39.5°C to 44.0°C) with Linear Interpolation (LERP)
 *  - Emergency Thermal Cutoff (>= 44.0°C) with 2.5°C Deadband Auto-Recovery
 *  - Time-Normalized Asymmetric Slew Pacing: 250 mA/s Derate, 350 mA/s Restore
 *  - Clean Handshake Settling Window (6s) for stable Plug -> Slow -> Fast -> Turbo (6A)
 *  - Functional Screen Boost: Unconstrains knots and MSCC when high speed is requested
 *  - 100% Target Cut-off with seamless transition into Hardware Bypass
 *  - Motherboard Keep-Alive: 3.0A USB-ICL hold prevents battery draw
 *  - Native Deep-Sleep Optimization: Halts polling when disconnected
 */

#include "aether.h"
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/pmic-voter.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>

#define AETHER_CHARGE_HOLD_CLIENT   "AETHER_CHARGE_HOLD_VOTER"
#define AETHER_THERMAL_TURBO_CLIENT "AETHER_THERMAL_TURBO_VOTER"

/* Charging current limit (uA, 0 = Stock 33W Unlimited) */
int aether_charge_current_limit = 0;

/* Native Xiaomi PMIC charge-hold controller */
static int aether_charge_cutoff_limit = 100; /* Default 100 = Full 33W Turbo to 100%, then hold in Bypass */
static int aether_charge_cutoff_mode = 1;    /* 0 = Cool Standby, 1 = Real Hardware Bypass */
static int aether_instant_bypass;            /* 0 = Normal, 1 = Instant Direct USB Power */
static int aether_screen_boost;              /* 0 = Cool Screen-Aware Cap, 1 = Uncapped Turbo */
static bool aether_cutoff_hardware_active;
static char aether_cutoff_status_buf[96] = "STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)";
static struct delayed_work aether_cutoff_work;

static DEFINE_MUTEX(aether_power_mutex);

/* Handshake timestamp tracking */
static unsigned long aether_plug_time_jiffies;
static bool aether_was_power_connected;

/*
 * Proactive Thermal Knot Point Definition (all temps in 0.1°C, currents in uA)
 * Designed specifically around Redmi Note 10 (SMB5 + BQ2597x) efficiency curves:
 *  - TI BQ2597x 2:1 Charge Pump operates at ~97.3% efficiency between 2.8A and 5.0A.
 *  - Currents between 1.8A and 2.5A force fallback to the Qualcomm SMB5 buck
 *    regulator at high current, causing severe motherboard/PMIC heating.
 *  - We maintain full 33W Turbo (6.0A) up to 39.5°C to preserve fast 0-50% times,
 *    smoothly glide across 40.5°C - 41.8°C keeping the BQ2597x in its sweet-spot,
 *    and if temperature exceeds 42.4°C, step cleanly down to <= 1.6A where the
 *    SMB5 buck converter operates with minimal thermal dissipation.
 */
struct aether_thermal_knot {
	int temp_tenth_c;  /* 395 = 39.5°C */
	int target_fcc_ua; /* 6000000 = 6.0A */
};

static const struct aether_thermal_knot aether_knots[] = {
	{ 395, 6000000 }, /* <= 39.5°C: 6.0A Full 33W Hardware Turbo */
	{ 405, 5000000 }, /*    40.5°C: 5.0A High-Efficiency Charge Pump Cruise */
	{ 412, 4000000 }, /*    41.2°C: 4.0A Fast Charge Glide */
	{ 418, 2800000 }, /*    41.8°C: 2.8A Minimum 2:1 Charge Pump Sweet-Spot Hold */
	{ 424, 1600000 }, /*    42.4°C: 1.6A Cool SMB5 Buck Mode (skips SMB5 hot zone) */
	{ 430, 1000000 }, /*    43.0°C: 1.0A High-Temp Cell & Motherboard Protection */
	{ 436,  500000 }, /*    43.6°C: 0.5A Minimal Trickle */
	{ 440,       0 }, /* >= 44.0°C: 0.0A Emergency Thermal Suspend Cutoff */
};
#define AETHER_KNOTS_COUNT ARRAY_SIZE(aether_knots)

/*
 * Dynamic Multi-Stage Constant Current (MSCC) Table
 * Lithium-ion internal resistance (R_dc) and anode overpotential increase
 * at higher state of charge. Gentle mid-SOC tapering keeps cell health optimal
 * while preserving full 33W turbo speeds during low-SOC bursts.
 */
struct aether_mscc_stage {
	int max_soc;
	int ceiling_fcc_ua;
};

static const struct aether_mscc_stage aether_mscc_table[] = {
	{  45, 6000000 }, /*  0% - 45%: 6.0A Low-Impedance Peak Turbo Burst */
	{  65, 5200000 }, /* 46% - 65%: 5.2A High-Efficiency Stage */
	{  78, 4200000 }, /* 66% - 78%: 4.2A Mid-SOC Stage */
	{  88, 3000000 }, /* 79% - 88%: 3.0A Pre-CV Low-Stress Stage */
};
#define AETHER_MSCC_COUNT ARRAY_SIZE(aether_mscc_table)

/* Dynamic PD + MSCC pacing controller state */
struct aether_thermal_controller {
	int last_filtered_temp;
	int prev_window_temp;
	int thermal_velocity_tenth_c; /* dT over velocity window (0.1°C) */
	int effective_pred_temp;      /* PD predicted temperature (0.1°C) */
	int pmic_temp_tenth_c;        /* Motherboard / PMIC secondary temp */
	int last_applied_fcc_ua;
	int peak_temp_during_derate;
	unsigned long last_step_jiffies;
	unsigned long last_vel_window_jiffies;
	bool is_derating;
	bool is_thermal_suspended;
};

static struct aether_thermal_controller aether_tc;

#define SLEW_DOWN_UA_PER_SEC   250000  /* 250 mA/sec smooth step-down */
#define SLEW_UP_UA_PER_SEC     350000  /* 350 mA/sec controlled recovery */
#define HYSTERESIS_TENTH_C          4  /* 0.4°C relaxation required before stepping up */
#define VELOCITY_WINDOW_MS      10000  /* 10-second window for dT/dt derivative calculation */
#define KD_DERIVATIVE_GAIN          3  /* Predictive horizon multiplier for positive dT/dt */
#define MAX_PREDICTIVE_LEAD_C      18  /* Cap predictive lead to +1.8°C max */

/*
 * aether_calc_mscc_ceiling_fcc:
 * Returns the maximum electrochemically optimal current for a given SOC.
 * When Screen Boost is active, MSCC is bypassed to deliver maximum charging current.
 */
static int aether_calc_mscc_ceiling_fcc(int soc)
{
	int i;

	if (READ_ONCE(aether_screen_boost))
		return aether_knots[0].target_fcc_ua;

	if (soc <= 0)
		return aether_knots[0].target_fcc_ua;

	for (i = 0; i < AETHER_MSCC_COUNT; i++) {
		if (soc <= aether_mscc_table[i].max_soc)
			return aether_mscc_table[i].ceiling_fcc_ua;
	}

	return aether_mscc_table[AETHER_MSCC_COUNT - 1].ceiling_fcc_ua;
}

/*
 * aether_calc_interpolated_fcc:
 * Calculates target fast-charging current using linear interpolation between knots.
 * Returns -1 for cold protection (< 10°C) to yield to OEM JEITA.
 * When Screen Boost is enabled, applies a +1.5°C thermal headroom offset.
 */
static int aether_calc_interpolated_fcc(int temp)
{
	int i;

	/* Cold protection (< 10°C): Let OEM JEITA handle cold protection natively */
	if (temp < 100)
		return -1;

	/* Screen Boost offset: allows +1.5°C extra thermal headroom when enabled */
	if (READ_ONCE(aether_screen_boost))
		temp = max(100, temp - 15);

	/* Optimal Turbo zone */
	if (temp <= aether_knots[0].temp_tenth_c)
		return aether_knots[0].target_fcc_ua;

	/* High temp emergency cutoff: >= 44.0°C */
	if (temp >= aether_knots[AETHER_KNOTS_COUNT - 1].temp_tenth_c)
		return 0;

	/* Smooth linear interpolation between knot points */
	for (i = 0; i < AETHER_KNOTS_COUNT - 1; i++) {
		if (temp >= aether_knots[i].temp_tenth_c &&
		    temp <= aether_knots[i + 1].temp_tenth_c) {
			int t0 = aether_knots[i].temp_tenth_c;
			int t1 = aether_knots[i + 1].temp_tenth_c;
			int i0 = aether_knots[i].target_fcc_ua;
			int i1 = aether_knots[i + 1].target_fcc_ua;

			return i0 + ((temp - t0) * (i1 - i0)) / (t1 - t0);
		}
	}

	return 0;
}

/*
 * aether_read_motherboard_temp:
 * Queries the TI BQ2597x charge pump die or Qualcomm SMB5 PMIC charger thermistor.
 * Returns temperature in 0.1°C, or 0 if unavailable.
 */
static int aether_read_motherboard_temp(void)
{
	struct power_supply *psy;
	union power_supply_propval pval = {0};
	int temp = 0;

	/* 1. Try TI BQ2597x switched-capacitor charge pump die temperature */
	psy = power_supply_get_by_name("bq2597x-master");
	if (!psy)
		psy = power_supply_get_by_name("bq2597x-standalone");

	if (psy) {
		if (power_supply_get_property(psy, POWER_SUPPLY_PROP_TI_DIE_TEMPERATURE, &pval) == 0) {
			temp = pval.intval;
			/* Normalize to 0.1°C if driver returns whole degrees */
			if (temp > 0 && temp <= 100)
				temp *= 10;
		}
		power_supply_put(psy);
	}

	/* 2. Fall back to Qualcomm SMB5 PMIC charger thermistor (decidegrees C) */
	if (temp <= 0 || temp > 850) {
		psy = power_supply_get_by_name("battery");
		if (psy) {
			if (power_supply_get_property(psy, POWER_SUPPLY_PROP_CHARGER_TEMP, &pval) == 0)
				temp = pval.intval;
			power_supply_put(psy);
		}
	}

	/* Sanity check valid range (10.0°C to 85.0°C) */
	if (temp < 100 || temp > 850)
		return 0;

	return temp;
}

/*
 * aether_reset_thermal_controller_locked:
 * Resets internal PD-MSCC tracking state when unplugged or entering bypass.
 */
static void aether_reset_thermal_controller_locked(int default_fcc_ua)
{
	aether_tc.last_applied_fcc_ua = default_fcc_ua;
	aether_tc.thermal_velocity_tenth_c = 0;
	aether_tc.effective_pred_temp = aether_tc.last_filtered_temp;
	aether_tc.peak_temp_during_derate = 0;
	aether_tc.last_step_jiffies = jiffies;
	aether_tc.last_vel_window_jiffies = jiffies;
	aether_tc.prev_window_temp = aether_tc.last_filtered_temp;
	aether_tc.is_derating = false;
	aether_tc.is_thermal_suspended = false;
}

/*
 * aether_is_bypass_active:
 * Exported symbol queried by Qualcomm SMB5 and TI BQ2597x drivers.
 */
bool aether_is_bypass_active(void)
{
	return READ_ONCE(aether_instant_bypass) ||
	       READ_ONCE(aether_cutoff_hardware_active);
}
EXPORT_SYMBOL(aether_is_bypass_active);

/*
 * aether_notify_usb_change:
 * Exported symbol invoked when USB cable is inserted/removed.
 * Wakes up the workqueue immediately for instant reaction.
 */
void aether_notify_usb_change(void)
{
	WRITE_ONCE(aether_plug_time_jiffies, jiffies);
	mod_delayed_work(system_wq, &aether_cutoff_work, 0);
}
EXPORT_SYMBOL(aether_notify_usb_change);

/*
 * aether_set_cp_charging_enabled:
 * Enables or disables TI BQ2597x switched-capacitor charge pumps.
 * Restored upon exit from bypass to ensure instant 33W Turbo charge.
 */
static void aether_set_cp_charging_enabled(int enable)
{
	struct power_supply *cp_psy;
	union power_supply_propval val = { .intval = enable };

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

/*
 * aether_set_hardware_bypass_locked:
 * Low-level hardware controller for switching between normal charging and bypass mode.
 * Must be called with aether_power_mutex held.
 */
static int aether_set_hardware_bypass_locked(bool disable)
{
	struct votable *chg_dis = find_votable("CHG_DISABLE");
	struct votable *usb_icl = find_votable("USB_ICL");
	struct votable *fcc = find_votable("FCC");
	struct power_supply *batt_psy = power_supply_get_by_name("battery");
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	struct power_supply *bms_psy = power_supply_get_by_name("bms");
	union power_supply_propval chg_val = { .intval = disable ? 0 : 1 };
	union power_supply_propval zero_val = { .intval = 0 };
	int ret = 0;

	if (disable) {
		/*
		 * 1. ENTRANCE TO BYPASS:
		 * Instantly revoke thermal turbo voter so there is zero voter conflict
		 * or residual vote on the FCC line while in bypass mode.
		 */
		if (fcc)
			vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
		if (chg_dis)
			vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
		if (usb_icl)
			vote(usb_icl, AETHER_THERMAL_TURBO_CLIENT, false, 0);

		aether_reset_thermal_controller_locked(0);

		/*
		 * 2. Cut battery charging at PMIC silicon level:
		 */
		if (chg_dis)
			vote(chg_dis, AETHER_CHARGE_HOLD_CLIENT, true, 0);
		if (fcc)
			vote(fcc, AETHER_CHARGE_HOLD_CLIENT, true, 0);

		/*
		 * 3. Motherboard power keep-alive (3.0A USB ICL):
		 */
		if (usb_icl)
			vote(usb_icl, AETHER_CHARGE_HOLD_CLIENT, true, 3000000);

		/*
		 * 4. Disable BQ2597x charge pump during bypass:
		 */
		aether_set_cp_charging_enabled(0);
	} else {
		/*
		 * 1. EXIT FROM BYPASS (RESTORING NORMAL CHARGING):
		 * Release bypass hardware clamps:
		 */
		if (chg_dis)
			vote(chg_dis, AETHER_CHARGE_HOLD_CLIENT, false, 0);
		if (fcc)
			vote(fcc, AETHER_CHARGE_HOLD_CLIENT, false, 0);
		if (usb_icl)
			vote(usb_icl, AETHER_CHARGE_HOLD_CLIENT, false, 0);

		/*
		 * 2. Re-enable BQ2597x charge pump for 33W Turbo Charge:
		 */
		aether_set_cp_charging_enabled(1);

		/*
		 * 3. Reset handshake timestamp so 33W Turbo can re-establish smoothly:
		 */
		aether_plug_time_jiffies = jiffies;

		/*
		 * 4. Release thermal turbo vote completely so OEM 33W Turbo handshake
		 * can establish freely and smoothly without restriction:
		 */
		if (chg_dis)
			vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
		if (usb_icl)
			vote(usb_icl, AETHER_THERMAL_TURBO_CLIENT, false, 0);
		if (fcc) {
			vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
			aether_reset_thermal_controller_locked(aether_knots[0].target_fcc_ua);
		}
	}

	/* Power supply sysfs properties synchronization */
	if (batt_psy) {
		power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_INPUT_SUSPEND, &zero_val);
		power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &chg_val);
		if (!disable) {
			union power_supply_propval fast_val = { .intval = 1 };
			power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &chg_val);
			power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_FASTCHARGE_MODE, &fast_val);
		}
		power_supply_changed(batt_psy);
		power_supply_put(batt_psy);
	}

	if (usb_psy) {
		power_supply_set_property(usb_psy, POWER_SUPPLY_PROP_INPUT_SUSPEND, &zero_val);
		power_supply_changed(usb_psy);
		power_supply_put(usb_psy);
	}

	if (bms_psy) {
		power_supply_changed(bms_psy);
		power_supply_put(bms_psy);
	}

	return ret;
}

static int __maybe_unused aether_set_hardware_bypass(bool disable)
{
	int ret;

	mutex_lock(&aether_power_mutex);
	ret = aether_set_hardware_bypass_locked(disable);
	mutex_unlock(&aether_power_mutex);
	return ret;
}

/*
 * aether_is_power_connected:
 * Multi-layer query across usb, main, and battery power supplies.
 */
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
			if (pval.intval == POWER_SUPPLY_STATUS_CHARGING)
				connected = true;
		}
		power_supply_put(psy);
	}

	return connected;
}

/*
 * aether_cutoff_workfunc:
 * Main background workqueue running the predictive thermal pacing loop,
 * SOC impedance stage matching, and hardware bypass switching.
 */
static void aether_cutoff_workfunc(struct work_struct *work)
{
	struct power_supply *batt_psy, *bms_psy;
	union power_supply_propval pval = {0};
	int soc = -1;
	int batt_status = -1;
	int ibat_ua = 0;
	bool power_connected = aether_is_power_connected();
	unsigned int next_delay_ms = 1000;

	mutex_lock(&aether_power_mutex);

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		if (power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &pval) == 0)
			soc = pval.intval;
		if (power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_STATUS, &pval) == 0)
			batt_status = pval.intval;
		if (power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval) == 0)
			ibat_ua = pval.intval;
		power_supply_put(batt_psy);
	}

	/* If phone is completely unplugged and user did NOT force instant bypass, restore charging */
	if (!power_connected && !aether_instant_bypass) {
		aether_was_power_connected = false;
		if (aether_cutoff_hardware_active) {
			aether_cutoff_hardware_active = false;
			aether_set_hardware_bypass_locked(false);
		}
		/* Drop thermal votes when disconnected */
		{
			struct votable *fcc = find_votable("FCC");
			struct votable *chg_dis = find_votable("CHG_DISABLE");
			struct votable *usb_icl = find_votable("USB_ICL");

			if (fcc)
				vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
			if (chg_dis)
				vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
			if (usb_icl)
				vote(usb_icl, AETHER_THERMAL_TURBO_CLIENT, false, 0);

			aether_tc.last_filtered_temp = 0;
			aether_reset_thermal_controller_locked(0);
		}
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"DISCONNECTED (Stock Turbo Charging Ready)");

		mutex_unlock(&aether_power_mutex);

		/* Stop workqueue when unplugged to allow full SoC PC3/AOSS deep sleep */
		return;
	}

	if (power_connected && !aether_was_power_connected) {
		aether_plug_time_jiffies = jiffies;
		aether_was_power_connected = true;
		aether_tc.last_step_jiffies = jiffies;
		aether_tc.last_vel_window_jiffies = jiffies;
	}

	/* Determine target state based on user settings and battery SOC */
	{
		bool target_disable = false;
		const char *target_status = "STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)";

		if (aether_instant_bypass) {
			target_disable = true;
			target_status = "CHARGE_HOLD_ACTIVE (Instant mode; USB input available)";
		} else if (aether_charge_cutoff_limit > 0 && soc > 0) {
			if (aether_charge_cutoff_limit < 100) {
				/* Sub-100% target (e.g. 80%, 85%, 90%, 95%) */
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
				/*
				 * 100% Target Cutoff (Default):
				 * Full PD-MSCC Turbo charge and natural CV taper to 100%.
				 * Only enters bypass once battery is truly 100% full and current tapered.
				 */
				if (soc >= 100) {
					if (batt_status == POWER_SUPPLY_STATUS_FULL || abs(ibat_ua) < 500000) {
						target_disable = true;
						target_status = "CHARGE_HOLD_ACTIVE (100% Full; Holding in Bypass)";
					} else if (aether_cutoff_hardware_active) {
						target_disable = true;
						target_status = "CHARGE_HOLD_ACTIVE (100% Bypass Active)";
					} else {
						target_disable = false;
						target_status = "CHARGING_ACTIVE (Completing 100% CV Top-Off)";
					}
				} else if (aether_cutoff_hardware_active && soc >= 98) {
					target_disable = true;
					target_status = "CHARGE_HOLD_ACTIVE (Hysteresis; 100% Bypass Hold)";
				} else {
					target_disable = false;
					target_status = "CHARGING_ACTIVE (PD-MSCC Cool Turbo Charging to 100%)";
				}
			}
		} else {
			target_disable = false;
			target_status = "CHARGING_ACTIVE (PD-MSCC Cool Turbo Charging Active)";
		}

		/* Apply bypass transitions */
		if (target_disable) {
			if (!aether_cutoff_hardware_active) {
				aether_cutoff_hardware_active = true;
				if (aether_set_hardware_bypass_locked(true)) {
					aether_cutoff_hardware_active = false;
					target_status = "CHARGE_HOLD_ERROR (charger votable unavailable)";
				}
			}
			snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf), "%s", target_status);
			next_delay_ms = 10000; /* Bypass holding steady: relax to 10s */
		} else {
			if (aether_cutoff_hardware_active) {
				aether_cutoff_hardware_active = false;
				if (aether_set_hardware_bypass_locked(false))
					target_status = "CHARGE_RESTORE_ERROR (charger votable unavailable)";
			}

			/*
			 * AETHER v1.6.1 PREDICTIVE PD-MSCC THERMAL & MOTHERBOARD PACING ENGINE:
			 * Runs when actively charging normally (bypass inactive).
			 */
			{
				struct votable *fcc = find_votable("FCC");
				struct votable *chg_dis = find_votable("CHG_DISABLE");
				struct votable *usb_icl = find_votable("USB_ICL");
				int raw_temp = 0, filtered_temp;
				int thermal_fcc, mscc_fcc, target_fcc, next_fcc;
				int turbo_temp_limit;
				unsigned long now = jiffies;
				unsigned int elapsed_ms;

				elapsed_ms = jiffies_to_msecs(now - aether_tc.last_step_jiffies);
				if (elapsed_ms < 100 || elapsed_ms > 5000)
					elapsed_ms = 1000;
				aether_tc.last_step_jiffies = now;

				if (soc >= 88 && aether_charge_current_limit == 0) {
					/*
					 * Above 88% SOC, battery is deep in the natural CV taper zone.
					 * Current naturally drops below 1.8A and heating is minimal.
					 * Revoke thermal turbo vote completely so the hardware PMIC
					 * CV loop has 100% pure, uninterrupted control down to 0mA at 100%.
					 */
					if (fcc)
						vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
					if (usb_icl)
						vote(usb_icl, AETHER_THERMAL_TURBO_CLIENT, false, 0);
					if (chg_dis && aether_tc.is_thermal_suspended) {
						vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
						aether_tc.is_thermal_suspended = false;
					}
					aether_tc.last_applied_fcc_ua = 0;
					aether_tc.is_derating = false;
					next_delay_ms = 2000;
				} else {
					bms_psy = power_supply_get_by_name("bms");
					if (!bms_psy)
						bms_psy = power_supply_get_by_name("battery");

					if (bms_psy) {
						if (power_supply_get_property(bms_psy, POWER_SUPPLY_PROP_TEMP, &pval) == 0)
							raw_temp = pval.intval;
						power_supply_put(bms_psy);
					}

					aether_tc.pmic_temp_tenth_c = aether_read_motherboard_temp();

					if (fcc && raw_temp > 0) {
						bool in_handshake;
						int lead_tenth_c = 0;

						/*
						 * 1. Time-Consistent EMA Noise Filter:
						 * Smooths thermistor ADC jitter while preserving thermal trends.
						 */
						if (aether_tc.last_filtered_temp == 0) {
							filtered_temp = raw_temp;
							aether_tc.prev_window_temp = raw_temp;
							aether_tc.last_vel_window_jiffies = now;
						} else {
							filtered_temp = (aether_tc.last_filtered_temp * 3 + raw_temp * 2) / 5;
						}
						aether_tc.last_filtered_temp = filtered_temp;

						/*
						 * 2. Derivative (dT/dt) Thermal Velocity Tracker:
						 * Evaluates temperature rise rate over a 10-second window.
						 * If battery is heating up rapidly, projects an effective
						 * predictive temperature (T_pred = T_filt + Kd * dT/dt)
						 * to brake current BEFORE heat saturates the battery and PCB.
						 */
						if (time_after_eq(now, aether_tc.last_vel_window_jiffies +
								  msecs_to_jiffies(VELOCITY_WINDOW_MS))) {
							aether_tc.thermal_velocity_tenth_c =
								filtered_temp - aether_tc.prev_window_temp;
							aether_tc.prev_window_temp = filtered_temp;
							aether_tc.last_vel_window_jiffies = now;
						}

						if (aether_tc.thermal_velocity_tenth_c > 0 && filtered_temp >= 360) {
							lead_tenth_c = aether_tc.thermal_velocity_tenth_c * KD_DERIVATIVE_GAIN;
							if (lead_tenth_c > MAX_PREDICTIVE_LEAD_C)
								lead_tenth_c = MAX_PREDICTIVE_LEAD_C;
						}

						/*
						 * Motherboard / Charge-Pump Die Guard:
						 * If secondary charger IC thermistor exceeds 48.0°C, add proportional
						 * thermal weight so motherboard stays cool.
						 */
						if (aether_tc.pmic_temp_tenth_c >= 480) {
							int pcb_strain = (aether_tc.pmic_temp_tenth_c - 480) / 3;
							if (pcb_strain > 15)
								pcb_strain = 15;
							if (pcb_strain > lead_tenth_c)
								lead_tenth_c = pcb_strain;
						}

						aether_tc.effective_pred_temp = filtered_temp + lead_tenth_c;

						/*
						 * 3. High-Temperature Emergency Safety (>= 44.0°C real temp) with Auto-Recovery:
						 * Uses real filtered_temp (not predictive) for hard cutoffs to avoid false trips.
						 * Auto-recovery requires cooling down to <= 41.5°C before resuming.
						 */
						if (filtered_temp >= 440 || aether_tc.is_thermal_suspended) {
							if (filtered_temp >= 440) {
								aether_tc.is_thermal_suspended = true;
								if (chg_dis)
									vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, true, 0);
								vote(fcc, AETHER_THERMAL_TURBO_CLIENT, true, 0);
								aether_tc.last_applied_fcc_ua = 0;
								target_status = "THERMAL_SUSPEND (Battery >= 44.0°C; Cooling Active)";
								next_delay_ms = 500;
								goto skip_fcc_handshake;
							} else if (filtered_temp <= 415) {
								/* Cooled down to <= 41.5°C: Auto-recover charging smoothly */
								aether_tc.is_thermal_suspended = false;
								if (chg_dis)
									vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
								aether_tc.last_applied_fcc_ua =
									aether_calc_interpolated_fcc(filtered_temp);
							} else {
								/* Still cooling in 41.6°C - 43.9°C recovery deadband */
								target_status = "THERMAL_SUSPEND (Cooling down to 41.5°C to recover)";
								next_delay_ms = 500;
								goto skip_fcc_handshake;
							}
						}

						/*
						 * 4. Compute Combined Target FCC:
						 *    min(PD_Thermal_FCC, MSCC_SOC_Ceiling, User_Limit)
						 */
						thermal_fcc = aether_calc_interpolated_fcc(aether_tc.effective_pred_temp);

						/* Cold protection (< 10.0°C): Release vote, let OEM JEITA handle */
						if (thermal_fcc < 0) {
							vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							if (chg_dis)
								vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							if (usb_icl)
								vote(usb_icl, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							aether_tc.last_applied_fcc_ua = 0;
							aether_tc.is_derating = false;
							target_status = "COLD_PROTECTION (OEM JEITA Active)";
							next_delay_ms = 2000;
							goto skip_fcc_handshake;
						}

						mscc_fcc = aether_calc_mscc_ceiling_fcc(soc);
						target_fcc = min(thermal_fcc, mscc_fcc);

						if (aether_charge_current_limit > 0 &&
						    aether_charge_current_limit < target_fcc)
							target_fcc = aether_charge_current_limit;

						/* Effective unconstrained Turbo temperature ceiling (+1.5°C when Screen Boost is enabled) */
						turbo_temp_limit = aether_knots[0].temp_tenth_c +
							(READ_ONCE(aether_screen_boost) ? 15 : 0);

						/*
						 * 5. Plug-In Handshaking Settling Window (6s):
						 * Allow hardware (SMB5 + BQ2597x) to cleanly negotiate
						 * QC3/PD PPS and ramp to 2:1 charge pump mode without
						 * any premature voting interference when plugged in cool.
						 */
						in_handshake = time_before(now,
							aether_plug_time_jiffies + msecs_to_jiffies(6000));
						if (in_handshake && filtered_temp <= turbo_temp_limit &&
						    aether_charge_current_limit == 0) {
							vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							if (chg_dis)
								vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							if (usb_icl)
								vote(usb_icl, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							aether_tc.last_applied_fcc_ua = aether_knots[0].target_fcc_ua;
							aether_tc.is_derating = false;
							target_status = "CHARGING_HANDSHAKE (Establishing 33W Turbo Handshake)";
							next_delay_ms = 1000;
							goto skip_fcc_handshake;
						}

						/*
						 * 6. BQ2597x Charge Pump vs SMB5 Buck Hot-Zone Guard:
						 * On Redmi Note 10, currents between 1.7A and 2.6A cause the
						 * BQ2597x 2:1 charge pump to drop out and overload the SMB5 buck
						 * converter at 9V/11V, heating the motherboard. Snap out of that deadzone!
						 */
						if (target_fcc > 1700000 && target_fcc < 2600000) {
							if (filtered_temp >= 420)
								target_fcc = 1600000; /* Cool SMB5 buck mode */
							else
								target_fcc = 2800000; /* Keep BQ2597x 2:1 pump alive */
						}

						/*
						 * 7. Hysteresis Anti-Hunting Deadband (0.4°C):
						 * Prevents rapid current oscillation around knot boundaries.
						 */
						if (aether_tc.effective_pred_temp <= turbo_temp_limit) {
							aether_tc.is_derating = false;
							aether_tc.peak_temp_during_derate = 0;
						} else if (target_fcc > aether_tc.last_applied_fcc_ua &&
						       aether_tc.is_derating) {
							if (aether_tc.effective_pred_temp >
								(aether_tc.peak_temp_during_derate - HYSTERESIS_TENTH_C)) {
								target_fcc = aether_tc.last_applied_fcc_ua;
							} else {
								aether_tc.peak_temp_during_derate =
									aether_tc.effective_pred_temp;
							}
						} else if (target_fcc < aether_tc.last_applied_fcc_ua) {
							aether_tc.is_derating = true;
							if (aether_tc.effective_pred_temp >
								aether_tc.peak_temp_during_derate)
								aether_tc.peak_temp_during_derate =
									aether_tc.effective_pred_temp;
						}

						/*
						 * 8. Time-Normalized Asymmetric Slew-Rate Limiter:
						 * Scales max current step by actual elapsed milliseconds (dt).
						 */
						if (aether_tc.last_applied_fcc_ua == 0) {
							next_fcc = target_fcc;
						} else if (target_fcc < aether_tc.last_applied_fcc_ua) {
							int max_down = (SLEW_DOWN_UA_PER_SEC * elapsed_ms) / 1000;
							int delta = aether_tc.last_applied_fcc_ua - target_fcc;

							if (delta > max_down)
								next_fcc = aether_tc.last_applied_fcc_ua - max_down;
							else
								next_fcc = target_fcc;
						} else if (target_fcc > aether_tc.last_applied_fcc_ua) {
							int max_up = (SLEW_UP_UA_PER_SEC * elapsed_ms) / 1000;
							int delta = target_fcc - aether_tc.last_applied_fcc_ua;

							if (delta > max_up)
								next_fcc = aether_tc.last_applied_fcc_ua + max_up;
							else
								next_fcc = target_fcc;
						} else {
							next_fcc = target_fcc;
						}

						/*
						 * 9. Hardware Voter Application & SMB5 Input Current Pacing:
						 * When operating in cool SMB5 Buck fallback (<= 1.6A), also cap
						 * USB_ICL to 1.8A to minimize PMIC switching heat on the motherboard.
						 */
						if (aether_tc.effective_pred_temp <= turbo_temp_limit &&
						    next_fcc >= aether_knots[0].target_fcc_ua &&
						    aether_charge_current_limit == 0) {
							vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							if (chg_dis)
								vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							if (usb_icl)
								vote(usb_icl, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							aether_tc.last_applied_fcc_ua = aether_knots[0].target_fcc_ua;
							aether_tc.is_derating = false;
							target_status = "CHARGING_ACTIVE (Full 33W Turbo Charging Active)";
						} else {
							vote(fcc, AETHER_THERMAL_TURBO_CLIENT, true, next_fcc);
							if (usb_icl) {
								if (next_fcc <= 1600000)
									vote(usb_icl, AETHER_THERMAL_TURBO_CLIENT, true, 1800000);
								else
									vote(usb_icl, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							}
							aether_tc.last_applied_fcc_ua = next_fcc;
							target_status = "THERMAL_PACING (PD-MSCC Cool Current Regulation)";
						}

						/* Pacing rate: 500ms when actively regulating (>= turbo_temp_limit), 1000ms otherwise */
						next_delay_ms = (aether_tc.effective_pred_temp >= turbo_temp_limit) ? 500 : 1000;
					} else {
						next_delay_ms = 1000;
					}

skip_fcc_handshake: ;
				}
			}

			snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf), "%s", target_status);
		}
	}

	mutex_unlock(&aether_power_mutex);
	schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(next_delay_ms));
}

/* --- Sysfs Attribute Implementations --- */

/* Charging Current Limit (uA, 0 = Stock 33W Unlimited) */
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

	mutex_lock(&aether_power_mutex);
	aether_charge_current_limit = limit;
	mutex_unlock(&aether_power_mutex);

	batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		val.intval = (limit > 0) ? limit : -EINVAL;
		power_supply_set_property(batt_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &val);
		power_supply_changed(batt_psy);
		power_supply_put(batt_psy);
	}

	mod_delayed_work(system_wq, &aether_cutoff_work, 0);
	return count;
}
static struct kobj_attribute charge_current_limit_attr = __ATTR_RW(charge_current_limit);

static ssize_t aacc_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		"CHARGING_ENGINE=AETHER_PD_MSCC_TURBO_v1.6.1\n"
		"AACC_ACTIVE=%d\n"
		"CHARGING_MODE=%s\n"
		"STATUS=%s\n",
		aether_is_bypass_active() ? 1 : 0,
		aether_instant_bypass ? "INSTANT_BYPASS" :
		(aether_charge_cutoff_limit > 0 ? "TARGET_CUTOFF" : "PD_MSCC_TURBO_CHARGING"),
		aether_cutoff_status_buf);
}
static struct kobj_attribute aacc_status_attr = __ATTR_RO(aacc_status);

static ssize_t aacc_thermalguard_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int temp = aether_tc.last_filtered_temp;
	int pred_temp = aether_tc.effective_pred_temp;
	int vel = aether_tc.thermal_velocity_tenth_c;
	int fcc = aether_tc.last_applied_fcc_ua;
	int turbo_limit = aether_knots[0].temp_tenth_c + (aether_screen_boost ? 15 : 0);
	const char *mode_str;

	if (aether_is_bypass_active())
		mode_str = "BYPASS_STANDBY (Thermal voter paused; cell current 0A)";
	else if (aether_tc.is_thermal_suspended)
		mode_str = "THERMAL_SUSPEND (>= 44.0°C Safety Cutoff; Auto-Cooling)";
	else if (time_before(jiffies, aether_plug_time_jiffies + msecs_to_jiffies(6000)) &&
		 temp <= turbo_limit)
		mode_str = "CHARGING_HANDSHAKE (33W Turbo Handshaking Settling Window)";
	else if (fcc == 0 || pred_temp <= turbo_limit)
		mode_str = aether_screen_boost ?
			"TURBO_BOOST_UNCONSTRAINED (<= 41.0°C; Screen Boost Active)" :
			"TURBO_UNCONSTRAINED (<= 39.5°C; Full 33W Hardware Turbo)";
	else if (aether_tc.is_derating)
		mode_str = "PREDICTIVE_DERATING (PD + MSCC rate-limited stepdown)";
	else
		mode_str = "SMOOTH_RECOVERY (Hysteresis gradual restoration)";

	return sprintf(buf,
		"THERMALGUARD_ENGINE=AETHER_PD_MSCC_TURBO_v1.6.1\n"
		"STATE=%s\n"
		"BATTERY_TEMP_C=%d.%d\n"
		"PREDICTIVE_TEMP_C=%d.%d\n"
		"THERMAL_VELOCITY_10S_C=%s%d.%d\n"
		"MOTHERBOARD_IC_TEMP_C=%d.%d\n"
		"TARGET_FCC_MA=%d\n"
		"CURRENT_VOTED_FCC_MA=%d\n"
		"HYSTERESIS_DEADBAND_C=0.4\n"
		"SLEW_DOWN_LIMIT_MA_S=250\n"
		"SLEW_UP_LIMIT_MA_S=350\n"
		"SCREEN_BOOST=%d\n"
		"STATUS=%s\n",
		mode_str,
		temp / 10, abs(temp % 10),
		pred_temp / 10, abs(pred_temp % 10),
		(vel < 0) ? "-" : "+", abs(vel) / 10, abs(vel) % 10,
		aether_tc.pmic_temp_tenth_c / 10, abs(aether_tc.pmic_temp_tenth_c % 10),
		aether_calc_interpolated_fcc(pred_temp) / 1000,
		fcc / 1000,
		aether_screen_boost,
		aether_cutoff_status_buf);
}
static struct kobj_attribute aacc_thermalguard_attr = __ATTR_RO(aacc_thermalguard);

static ssize_t aacc_screen_boost_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", aether_screen_boost);
}

static ssize_t aacc_screen_boost_store(struct kobject *kobj, struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	bool val;
	if (strtobool(buf, &val) < 0)
		return -EINVAL;

	aether_screen_boost = val ? 1 : 0;
	mod_delayed_work(system_wq, &aether_cutoff_work, 0);
	return count;
}
static struct kobj_attribute aacc_screen_boost_attr = __ATTR_RW(aacc_screen_boost);
static struct kobj_attribute screen_boost_attr = __ATTR(screen_boost, 0664, aacc_screen_boost_show, aacc_screen_boost_store);

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

	mutex_lock(&aether_power_mutex);
	aether_instant_bypass = val ? 1 : 0;
	if (aether_instant_bypass) {
		aether_cutoff_hardware_active = true;
		aether_set_hardware_bypass_locked(true);
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"CHARGE_HOLD_ACTIVE (Instant mode; USB input available)");
	} else {
		aether_cutoff_hardware_active = false;
		aether_set_hardware_bypass_locked(false);
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)");
	}
	mutex_unlock(&aether_power_mutex);

	mod_delayed_work(system_wq, &aether_cutoff_work, 0);
	return count;
}
static struct kobj_attribute aacc_idle_mode_attr = __ATTR_RW(aacc_idle_mode);

/* Charge Cutoff Limit Attribute (0 = Disabled, 50 - 100%) */
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

	if (val != 0 && (val < 50 || val > 100))
		return -EINVAL;

	aether_charge_cutoff_limit = val;
	mod_delayed_work(system_wq, &aether_cutoff_work, 0);
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
	mod_delayed_work(system_wq, &aether_cutoff_work, 0);
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

	mutex_lock(&aether_power_mutex);
	aether_instant_bypass = val ? 1 : 0;
	if (aether_instant_bypass) {
		aether_cutoff_hardware_active = true;
		aether_set_hardware_bypass_locked(true);
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"CHARGE_HOLD_ACTIVE (Direct Motherboard Power; USB Input Active)");
	} else {
		aether_cutoff_hardware_active = false;
		aether_set_hardware_bypass_locked(false);
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)");
	}
	mutex_unlock(&aether_power_mutex);

	mod_delayed_work(system_wq, &aether_cutoff_work, 0);
	return count;
}
static struct kobj_attribute instant_bypass_attr = __ATTR_RW(instant_bypass);

/* Charge Cutoff Status Attribute (Read-Only) */
static ssize_t charge_cutoff_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", aether_cutoff_status_buf);
}
static struct kobj_attribute charge_cutoff_status_attr = __ATTR_RO(charge_cutoff_status);

static struct attribute *aether_battery_attrs[] = {
	&charge_current_limit_attr.attr,
	&aacc_status_attr.attr,
	&aacc_thermalguard_attr.attr,
	&aacc_screen_boost_attr.attr,
	&screen_boost_attr.attr,
	&aacc_idle_mode_attr.attr,
	&charge_cutoff_limit_attr.attr,
	&charge_cutoff_mode_attr.attr,
	&instant_bypass_attr.attr,
	&charge_cutoff_status_attr.attr,
	NULL,
};

static const struct attribute_group aether_battery_attr_group = {
	.is_visible = aether_attr_is_visible,
	.attrs = aether_battery_attrs,
};

/*
 * aether_battery_init:
 * Initializes battery workqueue and registers sysfs attribute group.
 */
int aether_battery_init(struct kobject *parent)
{
	int ret;

	ret = sysfs_create_group(parent, &aether_battery_attr_group);
	if (ret) {
		pr_err("AETHER: Failed to register battery sysfs attributes (%d)\n", ret);
		return ret;
	}

	aether_tc.last_step_jiffies = jiffies;
	aether_tc.last_vel_window_jiffies = jiffies;

	INIT_DELAYED_WORK(&aether_cutoff_work, aether_cutoff_workfunc);
	schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(3000));

	{
		struct power_supply *bpsy = power_supply_get_by_name("battery");
		if (bpsy) {
			union power_supply_propval fval = { .intval = 1 };
			power_supply_set_property(bpsy, POWER_SUPPLY_PROP_FASTCHARGE_MODE, &fval);
			power_supply_put(bpsy);
		}
	}

	pr_info("AETHER: PD-MSCC Battery & Motherboard Thermal Pacing Subsystem v1.6.1 initialized\n");
	return 0;
}
