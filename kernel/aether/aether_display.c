// SPDX-License-Identifier: GPL-2.0
/*
 * AETHER Kernel Sandwiched Control Subsystem v1.6.1
 * Author: Kiran_Embedded
 *
 * Module: Display Enhancements & AMOLED Eye-Care Engine
 *
 * Responsibilities:
 *  - Sunlight High Brightness Mode (HBM): Boosts AMOLED panel drive to 1100 nits
 *  - AMOLED Anti-Flicker / DC Dimming: Eliminates PWM flicker at low brightness
 *  - Direct DRM DSI display hooks into drivers/gpu/drm/msm/dsi-staging/
 */

#include "aether.h"

/* 1. Sunlight HBM (High Brightness Mode 1100 Nits) */
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

/* 2. AMOLED Anti-Flicker / DC Dimming */
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

static struct attribute *aether_display_attrs[] = {
	&hbm_attr.attr,
	&anti_flicker_attr.attr,
	NULL,
};

static const struct attribute_group aether_display_attr_group = {
	.is_visible = aether_attr_is_visible,
	.attrs = aether_display_attrs,
};

/*
 * aether_display_init:
 * Registers display subsystem sysfs attribute group under /sys/kernel/aether.
 */
int aether_display_init(struct kobject *parent)
{
	int ret;

	ret = sysfs_create_group(parent, &aether_display_attr_group);
	if (ret) {
		pr_err("AETHER: Failed to register display sysfs attributes (%d)\n", ret);
		return ret;
	}

	pr_info("AETHER: Display (HBM & DC Dimming) Subsystem initialized\n");
	return 0;
}
