// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * KernelSU stock — core init / exit
 *
 * This file provides the module entry point for the stock (tiann)
 * KernelSU variant.  It mirrors the KernelSU-Next core/init.c
 * structure so the two variants can coexist in one tree without
 * touching each other's code.
 *
 * Build selection:  CONFIG_KSU_VARIANT_STOCK=y  (stock)
 *                   CONFIG_KSU_VARIANT_NEXT=y   (KernelSU-Next)
 * At most one variant is compiled per build — enforced by Kconfig.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kprobes.h>
#include <linux/version.h>
#include <linux/uidgid.h>
#include <linux/cred.h>
#include <linux/fs.h>

#include "include/ksu_stock.h"

/* ------------------------------------------------------------------ */
/*  Module metadata                                                    */
/* ------------------------------------------------------------------ */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("tiann / Aether Dual-KSU Integration");
MODULE_DESCRIPTION("KernelSU stock variant — UAPI v6");
MODULE_VERSION(__stringify(KSU_STOCK_VERSION));

/* ------------------------------------------------------------------ */
/*  Internal globals                                                   */
/* ------------------------------------------------------------------ */

/* Master UID allowlist — protected by ksu_stock_allowlist_lock */
static DEFINE_RWLOCK(ksu_stock_allowlist_lock);

struct ksu_stock_uid_entry {
	uid_t uid;
	bool  root_granted;
	bool  umount;
};

/* Simple fixed-size allowlist for early integration.
 * Production users should replace this with a dynamic xarray or rbtree. */
#define KSU_STOCK_MAX_UIDS  256
static struct ksu_stock_uid_entry ksu_stock_uid_table[KSU_STOCK_MAX_UIDS];
static int ksu_stock_uid_count;

static bool ksu_stock_safemode;

/* ------------------------------------------------------------------ */
/*  Allowlist helpers                                                  */
/* ------------------------------------------------------------------ */

bool ksu_stock_uid_is_granted_root(uid_t uid)
{
	int i;
	bool result = false;

	read_lock(&ksu_stock_allowlist_lock);
	for (i = 0; i < ksu_stock_uid_count; i++) {
		if (ksu_stock_uid_table[i].uid == uid) {
			result = ksu_stock_uid_table[i].root_granted;
			break;
		}
	}
	read_unlock(&ksu_stock_allowlist_lock);
	return result;
}
EXPORT_SYMBOL_GPL(ksu_stock_uid_is_granted_root);

bool ksu_stock_uid_should_umount(uid_t uid)
{
	int i;
	bool result = false;

	read_lock(&ksu_stock_allowlist_lock);
	for (i = 0; i < ksu_stock_uid_count; i++) {
		if (ksu_stock_uid_table[i].uid == uid) {
			result = ksu_stock_uid_table[i].umount;
			break;
		}
	}
	read_unlock(&ksu_stock_allowlist_lock);
	return result;
}
EXPORT_SYMBOL_GPL(ksu_stock_uid_should_umount);

/* ------------------------------------------------------------------ */
/*  Root grant                                                         */
/* ------------------------------------------------------------------ */

int ksu_stock_grant_root(void)
{
	struct cred *new_creds;

	if (!ksu_stock_uid_is_granted_root(
			from_kuid(&init_user_ns, current_uid()))) {
		return -EPERM;
	}

	new_creds = prepare_creds();
	if (!new_creds)
		return -ENOMEM;

	new_creds->uid = new_creds->euid = GLOBAL_ROOT_UID;
	new_creds->gid = new_creds->egid = GLOBAL_ROOT_GID;
	new_creds->suid = new_creds->sgid = GLOBAL_ROOT_UID;
	new_creds->fsuid = new_creds->fsgid = GLOBAL_ROOT_UID;

	/* Grant all capabilities */
	cap_fill(&new_creds->cap_inheritable);
	cap_fill(&new_creds->cap_permitted);
	cap_fill(&new_creds->cap_effective);
	cap_fill(&new_creds->cap_bset);

	return commit_creds(new_creds);
}
EXPORT_SYMBOL_GPL(ksu_stock_grant_root);

/* ------------------------------------------------------------------ */
/*  Safe-mode                                                          */
/* ------------------------------------------------------------------ */

bool ksu_stock_is_safe_mode(void)
{
	return READ_ONCE(ksu_stock_safemode);
}
EXPORT_SYMBOL_GPL(ksu_stock_is_safe_mode);

/* ------------------------------------------------------------------ */
/*  Manual hook entry points                                           */
/* ------------------------------------------------------------------ */

/*
 * ksu_stock_handle_execve — intercept execve for root detection.
 * Returns true if the call has been handled (caller should skip).
 */
bool ksu_stock_handle_execve(struct task_struct *task,
			     const char __user *filename,
			     const char __user *const __user *argv,
			     const char __user *const __user *envp,
			     struct pt_regs *regs)
{
	/* Placeholder: real implementation performs su binary detection
	 * and, if the caller is on the allowlist, prepares root creds.
	 * The KSU stock manager handles the full flow via IOCTL. */
	return false;
}
EXPORT_SYMBOL_GPL(ksu_stock_handle_execve);

bool ksu_stock_handle_vfs_read(struct file *file, char __user *buf,
			       size_t count, loff_t *pos)
{
	/* Placeholder: intercept reads for /proc/1/maps and similar
	 * paths to redirect to safe-mode or module-visible content. */
	return false;
}
EXPORT_SYMBOL_GPL(ksu_stock_handle_vfs_read);

bool ksu_stock_handle_faccessat(int dfd, const char __user *filename,
				int mode)
{
	/* Placeholder: intercept faccessat for su binary detection. */
	return false;
}
EXPORT_SYMBOL_GPL(ksu_stock_handle_faccessat);

bool ksu_stock_handle_newfstatat(int dfd, const char __user *filename,
				 struct kstat *stat, int flag)
{
	/* Placeholder: spoof stat results for su binary hiding. */
	return false;
}
EXPORT_SYMBOL_GPL(ksu_stock_handle_newfstatat);

/* ------------------------------------------------------------------ */
/*  SELinux helpers                                                    */
/* ------------------------------------------------------------------ */

void ksu_stock_selinux_init(void)
{
	/* Placeholder: set up permissive domain rules for ksu_d. */
}
EXPORT_SYMBOL_GPL(ksu_stock_selinux_init);

/* ------------------------------------------------------------------ */
/*  Module lifecycle                                                   */
/* ------------------------------------------------------------------ */

int ksu_stock_init(void)
{
	int err;

	pr_info("KernelSU stock v%d (UAPI=%d): initialising\n",
		KSU_STOCK_VERSION, KSU_STOCK_UAPI_VERSION);

	memset(ksu_stock_uid_table, 0, sizeof(ksu_stock_uid_table));
	ksu_stock_uid_count = 0;
	WRITE_ONCE(ksu_stock_safemode, false);

	ksu_stock_selinux_init();

	/* Register the /dev/ksu character device */
	err = ksu_stock_dev_init();
	if (err)
		return err;

	pr_info("KernelSU stock: ready\n");
	return 0;
}

void ksu_stock_exit(void)
{
	pr_info("KernelSU stock: exiting\n");
	ksu_stock_dev_exit();
}

static int __init ksu_stock_module_init(void)
{
	return ksu_stock_init();
}

static void __exit ksu_stock_module_exit(void)
{
	ksu_stock_exit();
}

module_init(ksu_stock_module_init);
module_exit(ksu_stock_module_exit);
