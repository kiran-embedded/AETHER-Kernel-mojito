// SPDX-License-Identifier: GPL-2.0
/*
 * AETHER Kernel Sandwiched Control Subsystem v1.6.1
 * Author: Kiran_Embedded
 *
 * Module: Smart Battery, Thermal Pacing & Hardware Bypass Engine
 *
 * Architecture & Hardware Control:
 *  - Qualcomm SMB5 PMIC + TI BQ2597x Switched-Capacitor Dual Charge Pump
 *  - Granular Thermal Knots (38.0°C to 45.0°C) with Linear Interpolation (LERP)
 *  - Emergency Thermal Cutoff (>= 45.0°C) with 1.0°C Deadband Auto-Recovery
 *  - Asymmetric Slew Pacing: 200 mA/s Derate, 150 mA/s Restore
 *  - 100% Target Cut-off with seamless transition into Hardware Bypass
 *  - Motherboard Keep-Alive: 3.0A USB-ICL hold prevents battery draw
 *  - Native Deep-Sleep Optimization: Halts polling when disconnected
 */

#include "aether.h"
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/pmic-voter.h>
#include <linux/mutex.h>

#define AETHER_CHARGE_HOLD_CLIENT   "AETHER_CHARGE_HOLD_VOTER"
#define AETHER_THERMAL_TURBO_CLIENT "AETHER_THERMAL_TURBO_VOTER"

/* Charging current limit (uA, 0 = Stock 33W Unlimited) */
int aether_charge_current_limit = 0;

/* Native Xiaomi PMIC charge-hold controller */
static int aether_charge_cutoff_limit = 100; /* Default 100 = Full 33W Turbo to 100%, then hold in Bypass */
static int aether_charge_cutoff_mode = 1;    /* 0 = Cool Standby, 1 = Real Hardware Bypass */
static int aether_instant_bypass;            /* 0 = Normal, 1 = Instant Direct USB Power */
static bool aether_cutoff_hardware_active;
static char aether_cutoff_status_buf[96] = "STOCK_CONTROL (Exact Xiaomi Turbo Charging Active)";
static struct delayed_work aether_cutoff_work;

static DEFINE_MUTEX(aether_power_mutex);

/*
 * Thermal knot point definition (all temps in 0.1°C, currents in uA)
 * Ultra-safe battery thermal curve:
 *   < 10°C:     OEM JEITA Cold Protection
 *   10°C–38°C:  6.0A Max Turbo (OEM unconstrained)
 *   38°C–39°C:  5.5A–6.0A (Begin thermal reduction)
 *   39°C–40°C:  4.5A (Smooth reduction)
 *   40°C–41°C:  3.5A (Warm protection)
 *   41°C–42°C:  2.5A (Strong reduction)
 *   42°C–43°C:  1.5A (High-temp protection)
 *   43°C–44°C:  0.8A (Minimal charging / trickle)
 *   >= 45.0°C:  0.0A (Emergency suspend with auto-recovery at <= 44.0°C)
 */
struct aether_thermal_knot {
	int temp_tenth_c;  /* 380 = 38.0°C */
	int target_fcc_ua; /* 6000000 = 6.0A */
};

static const struct aether_thermal_knot aether_knots[] = {
	{ 380, 6000000 }, /* <= 38.0°C: Full 6.0A Turbo (OEM unconstrained) */
	{ 390, 5500000 }, /*    39.0°C: 5.5A (Begin thermal reduction) */
	{ 400, 4500000 }, /*    40.0°C: 4.5A (Smooth reduction) */
	{ 410, 3500000 }, /*    41.0°C: 3.5A (Warm protection) */
	{ 420, 2500000 }, /*    42.0°C: 2.5A (Strong reduction) */
	{ 430, 1500000 }, /*    43.0°C: 1.5A (High-temp protection) */
	{ 440,  800000 }, /*    44.0°C: 0.8A (Minimal charging / trickle) */
	{ 450,       0 }, /* >= 45.0°C: 0.0A (Thermal suspend charging) */
};
#define AETHER_KNOTS_COUNT ARRAY_SIZE(aether_knots)

/* Dynamic pacing controller state */
struct aether_thermal_controller {
	int last_filtered_temp;
	int last_applied_fcc_ua;
	int peak_temp_during_derate;
	bool is_derating;
	bool is_thermal_suspended;
};

static struct aether_thermal_controller aether_tc;

#define SLEW_DOWN_MAX_UA     200000  /* 200 mA/sec step-down for smooth thermal safety */
#define SLEW_UP_MAX_UA       150000  /* 150 mA/sec gradual restore for fluid recovery without hunting */
#define HYSTERESIS_TENTH_C        5  /* 0.5°C relaxation required before stepping up */

/*
 * aether_calc_interpolated_fcc:
 * Calculates target fast-charging current using linear interpolation between knots.
 * Returns -1 for cold protection (< 10°C) to yield to OEM JEITA.
 */
static int aether_calc_interpolated_fcc(int temp)
{
	int i;

	/* Cold protection (< 10°C): Let OEM JEITA handle cold protection natively */
	if (temp < 100)
		return -1;

	/* Optimal Turbo zone: 10.0°C to 38.0°C */
	if (temp <= aether_knots[0].temp_tenth_c)
		return aether_knots[0].target_fcc_ua;

	/* High temp emergency cutoff: >= 45.0°C */
	if (temp >= aether_knots[AETHER_KNOTS_COUNT - 1].temp_tenth_c)
		return 0;

	/* Smooth linear interpolation between knot points (38.0°C - 45.0°C) */
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
	mod_delayed_work(system_wq, &aether_cutoff_work, 0);
}
EXPORT_SYMBOL(aether_notify_usb_change);

/*
 * aether_set_cp_charging_enabled:
 * Enables or disables TI BQ2597x switched-capacitor charge pumps.
 * Explicitly restored upon exit from bypass to enable Flash 2 charging.
 */
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
		 * ENTRANCE TO BYPASS:
		 * 1. Instantly revoke thermal turbo voter so there is zero voter conflict
		 *    or residual vote on the FCC line while in bypass mode.
		 */
		if (fcc)
			vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
		if (chg_dis)
			vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
		aether_tc.last_applied_fcc_ua = 0;
		aether_tc.is_derating = false;
		aether_tc.is_thermal_suspended = false;

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
		 * EXIT FROM BYPASS (RESTORING NORMAL CHARGING):
		 * 1. Release bypass hardware clamps:
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
		 * 3. Release thermal turbo vote completely so OEM 33W Turbo handshake
		 *    can establish freely and smoothly without restriction:
		 */
		if (chg_dis)
			vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
		if (fcc) {
			vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
			aether_tc.last_applied_fcc_ua = aether_knots[0].target_fcc_ua;
			aether_tc.is_derating = false;
			aether_tc.is_thermal_suspended = false;
		}
	}

	/* Power supply sysfs properties synchronization */
	if (batt_psy) {
		power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_INPUT_SUSPEND, &zero_val);
		power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &chg_val);
		if (!disable)
			power_supply_set_property(batt_psy, POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &chg_val);
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
			if (pval.intval == POWER_SUPPLY_STATUS_CHARGING ||
			    pval.intval == POWER_SUPPLY_STATUS_FULL ||
			    pval.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
				connected = true;
		}
		power_supply_put(psy);
	}

	return connected;
}

/*
 * aether_cutoff_workfunc:
 * Main background orchestrator. Runs every 1s (or 500ms when hot, 10s when bypass holds).
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
		if (aether_cutoff_hardware_active) {
			aether_cutoff_hardware_active = false;
			aether_set_hardware_bypass_locked(false);
		}
		/* Drop thermal vote when disconnected */
		{
			struct votable *fcc = find_votable("FCC");
			struct votable *chg_dis = find_votable("CHG_DISABLE");
			if (fcc)
				vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
			if (chg_dis)
				vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
			aether_tc.last_applied_fcc_ua = 0;
			aether_tc.is_derating = false;
			aether_tc.is_thermal_suspended = false;
		}
		snprintf(aether_cutoff_status_buf, sizeof(aether_cutoff_status_buf),
			"DISCONNECTED (Stock Turbo Charging Ready)");

		mutex_unlock(&aether_power_mutex);

		/* Stop workqueue when unplugged to allow full SoC PC3/AOSS deep sleep */
		return;
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
				 * Full unconstrained 33W Turbo charge and natural CV taper to 100%.
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
					target_status = "CHARGING_ACTIVE (Full Turbo Charging to 100%)";
				}
			}
		} else {
			target_disable = false;
			target_status = "CHARGING_ACTIVE (Smooth Thermal Turbo Charging Active)";
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
			 * AETHER SMOOTH THERMAL PACING STEP:
			 * Runs when actively charging normally (bypass inactive).
			 */
			{
				struct votable *fcc = find_votable("FCC");
				struct votable *chg_dis = find_votable("CHG_DISABLE");
				int raw_temp = 0, filtered_temp;
				int target_fcc, next_fcc;

				if (soc >= 75) {
					/*
					 * Above 75% SOC, battery is in the natural CV taper zone.
					 * Current naturally drops well below 2.5A and heating is minimal.
					 * Revoke thermal turbo vote completely so the hardware PMIC
					 * CV loop has 100% pure, uninterrupted control down to 0mA at 100%.
					 */
					if (fcc)
						vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
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

					if (fcc && raw_temp > 0) {
						/* EMA noise filter: 75% previous + 25% new sample */
						if (aether_tc.last_filtered_temp == 0)
							filtered_temp = raw_temp;
						else
							filtered_temp = (aether_tc.last_filtered_temp * 3 + raw_temp) / 4;
						aether_tc.last_filtered_temp = filtered_temp;

						/* 1. Calculate target FCC via LERP knots */
						target_fcc = aether_calc_interpolated_fcc(filtered_temp);

						/*
						 * 2. High-Temperature Emergency Safety (>= 45.0°C) with Auto-Recovery:
						 * If temperature reaches >= 45.0°C: suspend battery charging.
						 * Auto-recovery requires cooling down to <= 44.0°C before resuming.
						 */
						if (filtered_temp >= 450 || aether_tc.is_thermal_suspended) {
							if (filtered_temp >= 450) {
								aether_tc.is_thermal_suspended = true;
								if (chg_dis)
									vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, true, 0);
								vote(fcc, AETHER_THERMAL_TURBO_CLIENT, true, 0);
								aether_tc.last_applied_fcc_ua = 0;
								target_status = "THERMAL_SUSPEND (Battery >= 45°C; Cooling Active)";
								next_delay_ms = 500;
								goto skip_fcc_handshake;
							} else if (filtered_temp <= 440) {
								/* Cooled down to <= 44.0°C: Auto-recover charging smoothly */
								aether_tc.is_thermal_suspended = false;
								if (chg_dis)
									vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
								aether_tc.last_applied_fcc_ua = target_fcc;
							} else {
								/* Still cooling in 44.1°C - 44.9°C recovery deadband */
								target_status = "THERMAL_SUSPEND (Cooling down to 44°C to recover)";
								next_delay_ms = 500;
								goto skip_fcc_handshake;
							}
						}

						/* Cold protection (< 10.0°C): Release vote, let OEM JEITA handle */
						if (target_fcc < 0) {
							vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							if (chg_dis)
								vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							aether_tc.last_applied_fcc_ua = 0;
							aether_tc.is_derating = false;
							target_status = "COLD_PROTECTION (OEM JEITA Active)";
							next_delay_ms = 2000;
							goto skip_fcc_handshake;
						}

						/* 3. Hysteresis Anti-Hunting */
						if (target_fcc > aether_tc.last_applied_fcc_ua && aether_tc.is_derating) {
							if (filtered_temp > (aether_tc.peak_temp_during_derate - HYSTERESIS_TENTH_C)) {
								/* In deadband: hold current steady to prevent oscillation */
								target_fcc = aether_tc.last_applied_fcc_ua;
							} else {
								aether_tc.peak_temp_during_derate = filtered_temp;
							}
						} else if (target_fcc < aether_tc.last_applied_fcc_ua) {
							aether_tc.is_derating = true;
							if (filtered_temp > aether_tc.peak_temp_during_derate)
								aether_tc.peak_temp_during_derate = filtered_temp;
						}

						/* 4. Slew-Rate Limiting */
						if (aether_tc.last_applied_fcc_ua == 0) {
							next_fcc = target_fcc;
						} else if (target_fcc < aether_tc.last_applied_fcc_ua) {
							int delta = aether_tc.last_applied_fcc_ua - target_fcc;
							if (delta > SLEW_DOWN_MAX_UA)
								next_fcc = aether_tc.last_applied_fcc_ua - SLEW_DOWN_MAX_UA;
							else
								next_fcc = target_fcc;
						} else if (target_fcc > aether_tc.last_applied_fcc_ua) {
							int delta = target_fcc - aether_tc.last_applied_fcc_ua;
							if (delta > SLEW_UP_MAX_UA)
								next_fcc = aether_tc.last_applied_fcc_ua + SLEW_UP_MAX_UA;
							else
								next_fcc = target_fcc;
						} else {
							next_fcc = target_fcc;
						}

						/* 5. Hardware Voter Handshake */
						if (filtered_temp <= aether_knots[0].temp_tenth_c &&
						    next_fcc >= aether_knots[0].target_fcc_ua) {
							/* Sub-38°C and fully restored: release vote to OEM 33W turbo */
							vote(fcc, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							if (chg_dis)
								vote(chg_dis, AETHER_THERMAL_TURBO_CLIENT, false, 0);
							aether_tc.last_applied_fcc_ua = aether_knots[0].target_fcc_ua;
							aether_tc.is_derating = false;
						} else {
							vote(fcc, AETHER_THERMAL_TURBO_CLIENT, true, next_fcc);
							aether_tc.last_applied_fcc_ua = next_fcc;
						}

						/* Pacing rate: 500ms when hot (>=41.0°C), 1000ms otherwise */
						next_delay_ms = (filtered_temp >= 410) ? 500 : 1000;
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

	aether_charge_current_limit = limit;

	if (limit > 0) {
		batt_psy = power_supply_get_by_name("battery");
		if (batt_psy) {
			val.intval = limit;
			power_supply_set_property(batt_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &val);
			power_supply_changed(batt_psy);
			power_supply_put(batt_psy);
		}
	}
	return count;
}
static struct kobj_attribute charge_current_limit_attr = __ATTR_RW(charge_current_limit);

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
	int temp = aether_tc.last_filtered_temp;
	int fcc = aether_tc.last_applied_fcc_ua;
	const char *mode_str;

	if (aether_is_bypass_active())
		mode_str = "BYPASS_STANDBY (Thermal voter paused; cell current 0A)";
	else if (aether_tc.is_thermal_suspended)
		mode_str = "THERMAL_SUSPEND (>= 45.0°C Safety Cutoff; Auto-Cooling)";
	else if (fcc == 0 || temp <= aether_knots[0].temp_tenth_c)
		mode_str = "TURBO_UNCONSTRAINED (<= 38.0°C; Full 33W Hardware Turbo)";
	else if (aether_tc.is_derating)
		mode_str = "SMOOTH_DERATING (Active rate-limited stepdown)";
	else
		mode_str = "SMOOTH_RECOVERY (Hysteresis gradual restoration)";

	return sprintf(buf,
		"THERMALGUARD_ENGINE=AETHER_SMOOTH_TURBO_PACING\n"
		"STATE=%s\n"
		"BATTERY_TEMP_C=%d.%d\n"
		"TARGET_FCC_MA=%d\n"
		"CURRENT_VOTED_FCC_MA=%d\n"
		"HYSTERESIS_DEADBAND_C=0.5\n"
		"SLEW_DOWN_LIMIT_MA_S=200\n"
		"SLEW_UP_LIMIT_MA_S=150\n"
		"STATUS=%s\n",
		mode_str,
		temp / 10, temp % 10,
		aether_calc_interpolated_fcc(temp) / 1000,
		fcc / 1000,
		aether_cutoff_status_buf);
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

	INIT_DELAYED_WORK(&aether_cutoff_work, aether_cutoff_workfunc);
	schedule_delayed_work(&aether_cutoff_work, msecs_to_jiffies(3000));

	pr_info("AETHER: Battery & Thermal Pacing Subsystem initialized\n");
	return 0;
}
