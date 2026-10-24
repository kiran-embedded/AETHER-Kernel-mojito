// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * KernelSU stock — character device & IOCTL dispatcher
 *
 * Exposes /dev/ksu (or a prctl-based channel) and dispatches
 * IOCTL commands defined in KernelSU/uapi/sucompat.h.
 *
 * UAPI version reported: KSU_STOCK_UAPI_VERSION = 6
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cred.h>
#include <linux/uidgid.h>

#include "include/ksu_stock.h"
/* sucompat.h IOCTL defs — path from KernelSU/kernel/ to KernelSU/uapi/ */
#include "../uapi/sucompat.h"

/* ------------------------------------------------------------------ */
/*  Internal version constant (injected by Kbuild)                    */
/* ------------------------------------------------------------------ */
#ifndef KSU_STOCK_VERSION
# define KSU_STOCK_VERSION 1
#endif

/* ------------------------------------------------------------------ */
/*  IOCTL dispatcher                                                   */
/* ------------------------------------------------------------------ */

static long ksu_stock_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;

	switch (cmd) {

	/* ---- GET_INFO (v6, with uapi_version field) ---- */
	case KSU_STOCK_IOCTL_GET_INFO: {
		struct ksu_stock_get_info_cmd info = {
			.version      = KSU_STOCK_VERSION,
			.flags        = 0,
			.features     = 0,
			.uapi_version = KSU_STOCK_UAPI_VERSION, /* == 6 */
		};
		if (copy_to_user(argp, &info, sizeof(info)))
			return -EFAULT;
		break;
	}

	/* ---- GET_INFO (legacy — no uapi_version field) ---- */
	case KSU_STOCK_IOCTL_GET_INFO_LEGACY: {
		struct {
			__u32 version;
			__u32 flags;
			__u32 features;
		} legacy = {
			.version  = KSU_STOCK_VERSION,
			.flags    = 0,
			.features = 0,
		};
		if (copy_to_user(argp, &legacy, sizeof(legacy)))
			return -EFAULT;
		break;
	}

	/* ---- GRANT_ROOT ---- */
	case KSU_STOCK_IOCTL_GRANT_ROOT:
		ret = ksu_stock_grant_root();
		break;

	/* ---- UID_GRANTED_ROOT ---- */
	case KSU_STOCK_IOCTL_UID_GRANTED_ROOT: {
		struct ksu_stock_uid_granted_root_cmd q;
		if (copy_from_user(&q, argp, sizeof(q)))
			return -EFAULT;
		q.granted = ksu_stock_uid_is_granted_root(q.uid) ? 1 : 0;
		if (copy_to_user(argp, &q, sizeof(q)))
			return -EFAULT;
		break;
	}

	/* ---- UID_SHOULD_UMOUNT ---- */
	case KSU_STOCK_IOCTL_UID_SHOULD_UMOUNT: {
		struct ksu_stock_uid_should_umount_cmd q;
		if (copy_from_user(&q, argp, sizeof(q)))
			return -EFAULT;
		q.should_umount = ksu_stock_uid_should_umount(q.uid) ? 1 : 0;
		if (copy_to_user(argp, &q, sizeof(q)))
			return -EFAULT;
		break;
	}

	/* ---- CHECK_SAFEMODE ---- */
	case KSU_STOCK_IOCTL_CHECK_SAFEMODE: {
		struct ksu_stock_check_safemode_cmd q = {
			.in_safe_mode = ksu_stock_is_safe_mode() ? 1 : 0,
		};
		if (copy_to_user(argp, &q, sizeof(q)))
			return -EFAULT;
		break;
	}

	/* ---- REPORT_EVENT ---- */
	case KSU_STOCK_IOCTL_REPORT_EVENT: {
		struct ksu_stock_report_event_cmd ev;
		if (copy_from_user(&ev, argp, sizeof(ev)))
			return -EFAULT;
		pr_info("KernelSU stock: event %u reported\n", ev.event);
		break;
	}

	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

/* ------------------------------------------------------------------ */
/*  File operations                                                    */
/* ------------------------------------------------------------------ */

static const struct file_operations ksu_stock_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = ksu_stock_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ksu_stock_ioctl,
#endif
};

static struct miscdevice ksu_stock_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "ksu",
	.fops  = &ksu_stock_fops,
	.mode  = 0600,
};

/* ------------------------------------------------------------------ */
/*  Device lifecycle (called from core.c)                              */
/* ------------------------------------------------------------------ */

int ksu_stock_dev_init(void)
{
	int err = misc_register(&ksu_stock_miscdev);
	if (err) {
		pr_err("KernelSU stock: failed to register /dev/ksu (%d)\n",
		       err);
		return err;
	}
	pr_info("KernelSU stock: /dev/ksu registered (UAPI v%d)\n",
		KSU_STOCK_UAPI_VERSION);
	return 0;
}

void ksu_stock_dev_exit(void)
{
	misc_deregister(&ksu_stock_miscdev);
}
