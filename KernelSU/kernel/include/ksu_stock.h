/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * KernelSU stock — kernel-side public include
 * Included by kernel code that uses the stock KSU root engine.
 */
#ifndef __KSU_STOCK_KERNEL_H
#define __KSU_STOCK_KERNEL_H

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cred.h>

/* ------------------------------------------------------------------ */
/*  Version — injected by Kbuild via -DKSU_STOCK_VERSION=<N>          */
/* ------------------------------------------------------------------ */
#ifndef KSU_STOCK_VERSION
# define KSU_STOCK_VERSION  0
#endif
#define KERNEL_SU_STOCK_VERSION KSU_STOCK_VERSION

/* ------------------------------------------------------------------ */
/*  Core lifecycle                                                      */
/* ------------------------------------------------------------------ */
int  ksu_stock_init(void);
void ksu_stock_exit(void);

/* ------------------------------------------------------------------ */
/*  Root grant / credential helpers                                    */
/* ------------------------------------------------------------------ */
bool ksu_stock_uid_is_granted_root(uid_t uid);
bool ksu_stock_uid_should_umount(uid_t uid);
int  ksu_stock_grant_root(void);

/* ------------------------------------------------------------------ */
/*  Manual-hook entry points (called from patched kernel sites)        */
/* ------------------------------------------------------------------ */
bool ksu_stock_handle_execve(struct task_struct *task,
			     const char __user *filename,
			     const char __user *const __user *argv,
			     const char __user *const __user *envp,
			     struct pt_regs *regs);

bool ksu_stock_handle_vfs_read(struct file *file, char __user *buf,
			       size_t count, loff_t *pos);

bool ksu_stock_handle_faccessat(int dfd, const char __user *filename,
				int mode);

bool ksu_stock_handle_newfstatat(int dfd, const char __user *filename,
				 struct kstat *stat, int flag);

/* ------------------------------------------------------------------ */
/*  Device lifecycle (ksu_stock_dev.c)                                 */
/* ------------------------------------------------------------------ */
int  ksu_stock_dev_init(void);
void ksu_stock_dev_exit(void);

/* ------------------------------------------------------------------ */
/*  UAPI version — MUST remain 6 for stock KSU manager compatibility  */
/* ------------------------------------------------------------------ */
#define KSU_STOCK_UAPI_VERSION  6
bool ksu_stock_is_safe_mode(void);

/* ------------------------------------------------------------------ */
/*  SELinux helpers                                                    */
/* ------------------------------------------------------------------ */
void ksu_stock_selinux_init(void);

#endif /* __KSU_STOCK_KERNEL_H */
