// SPDX-License-Identifier: GPL-2.0
/*
 * AETHER Kernel Sandwiched Control Subsystem v1.6.1
 * Author: Kiran_Embedded
 *
 * Module: Performance, Memory Pacing & Low-Latency Network Engine
 *
 * Responsibilities:
 *  - TCP Bufferbloat & Lowat Pacing:
 *      Optimizes socket queuing to eliminate latency spikes under load
 *      Default: tcp_notsent_lowat = 16384 bytes, tcp_fastopen = 3
 *  - Dirty Page Stream Pacing:
 *      Gentle flusher wakeups (1000cs writeback / 2000cs expire) preventing
 *      sudden storage I/O lockups and dropped frames during heavy downloads
 *  - Sched Idle Search (Top-App Prefer Idle):
 *      Directs foreground interactive threads to immediate idle CPU cores
 */

#include "aether.h"
#include <linux/writeback.h>
#include <net/net_namespace.h>

int aether_sched_idle_search = 1;
EXPORT_SYMBOL(aether_sched_idle_search);

/* 1. TCP Lowat Pacing Attribute */
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

/* 2. Dirty Page Pacing Attribute */
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

/* 3. Sched Idle Search (Top-App Prefer Idle Pacing) */
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

static struct attribute *aether_perf_attrs[] = {
	&tcp_lowat_pacing_attr.attr,
	&dirty_page_pacing_attr.attr,
	&sched_idle_search_attr.attr,
	NULL,
};

static const struct attribute_group aether_perf_attr_group = {
	.is_visible = aether_attr_is_visible,
	.attrs = aether_perf_attrs,
};

/*
 * aether_perf_init:
 * Initializes in-kernel network low-latency defaults and registers sysfs attributes.
 */
int aether_perf_init(struct kobject *parent)
{
	int ret;

	ret = sysfs_create_group(parent, &aether_perf_attr_group);
	if (ret) {
		pr_err("AETHER: Failed to register perf sysfs attributes (%d)\n", ret);
		return ret;
	}

	/* In-kernel network low-latency and bufferbloat pacing defaults */
	init_net.ipv4.sysctl_tcp_fastopen = 3;
	init_net.ipv4.sysctl_tcp_notsent_lowat = 16384;

	pr_info("AETHER: Performance & Memory Pacing Subsystem initialized\n");
	return 0;
}
