// SPDX-License-Identifier: GPL-2.0
/*
 * AETHER Kernel Sandwiched Control Subsystem v1.6.1
 * Author: Kiran_Embedded
 *
 * Module: Pro Audio, Hardware Gain Calibration & Mic Filtering Engine
 *
 * Responsibilities:
 *  - Mic High-Pass Filter (HPF) modes:
 *      0 = Flat (Raw studio bypass)
 *      1 = 75Hz Vocal Clarity (Stock default)
 *      2 = 150Hz Wind & Ambient Noise Suppression
 *  - Mic Fast Unmute Delay: Eliminates initial voice call cutout (default 40ms)
 *  - Mic ADC Pre-Amp Calibration: Range 8 - 15 (default 12)
 *  - Talk / Bottom Mic Gain: Range -12 dB to +12 dB (default 0 dB)
 *  - Ambient / Top Mic Gain: Range -12 dB to +12 dB (default 0 dB)
 *  - Receiver Earpiece Volume Boost: Range 0 to 6 dB (default 0 dB)
 *  - HD2 Hi-Fi Mode: Ultra-low distortion headphone output switch
 */

#include "aether.h"

/* Exported in-kernel tuning states queried by Qualcomm Bolero audio codec */
int aether_audio_bypass_mode = 1;
EXPORT_SYMBOL(aether_audio_bypass_mode);

int aether_mic_hpf_mode = 1;
EXPORT_SYMBOL(aether_mic_hpf_mode);

int aether_mic_unmute_delay = 40;
int aether_mic_adc_gain = 12;
int aether_mic_gain_bottom = 0;
int aether_mic_gain_top = 0;
int aether_earpiece_boost = 0;
int aether_hifi_mode = 0;

/* 1. Audio Bypass Mode Attribute (Default 1: Stock Audio) */
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

/* 2. Mic HPF Noise Filter Mode (0: Flat, 1: 75Hz Vocal, 2: 150Hz Wind/Noise) */
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

/* 3. Mic Unmute Delay Attribute (in milliseconds, default 40ms) */
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

/* 4. Mic ADC Pre-Amp Gain (8 - 15, default 12) */
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

/* 5. Mic Bottom / Primary Talk Gain dB (-12 to +12, default 0) */
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

/* 6. Mic Top / Ambient Reference Gain dB (-12 to +12, default 0) */
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

/* 7. Receiver Earpiece Volume Boost dB (0 to 6, default 0) */
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

/* 8. Headphone Hi-Fi High-Performance Mode (0: Normal, 1: HD2 Hi-Fi) */
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

static struct attribute *aether_audio_attrs[] = {
	&audio_bypass_mode_attr.attr,
	&mic_hpf_mode_attr.attr,
	&mic_unmute_delay_attr.attr,
	&mic_adc_gain_attr.attr,
	&mic_gain_bottom_attr.attr,
	&mic_gain_top_attr.attr,
	&earpiece_boost_attr.attr,
	&hifi_mode_attr.attr,
	NULL,
};

static const struct attribute_group aether_audio_attr_group = {
	.is_visible = aether_attr_is_visible,
	.attrs = aether_audio_attrs,
};

/*
 * aether_audio_init:
 * Registers audio subsystem sysfs attribute group under /sys/kernel/aether.
 */
int aether_audio_init(struct kobject *parent)
{
	int ret;

	ret = sysfs_create_group(parent, &aether_audio_attr_group);
	if (ret) {
		pr_err("AETHER: Failed to register audio sysfs attributes (%d)\n", ret);
		return ret;
	}

	pr_info("AETHER: Pro Audio Subsystem initialized\n");
	return 0;
}
