/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __NET_REKERNEL_H
#define __NET_REKERNEL_H

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/cgroup.h>
#include <linux/freezer.h>

#define MIN_USERAPP_UID			10000
#define MAX_SYSTEM_UID			2000

#define RESERVE_ORDER			17
#define WARN_AHEAD_SPACE		(1 << RESERVE_ORDER)

#define INTERFACETOKEN_BUFF_SIZE	140
#define PARCEL_OFFSET			16 /* sync with writeInterfaceToken() */

#define LINE_ERROR			(-1)
#define LINE_SUCCESS			0

enum report_type {
	BINDER,
	SIGNAL,
};

enum binder_type {
	REPLY,
	TRANSACTION,
	OVERFLOW,
};

static inline bool frozen_task_group(struct task_struct *task)
{
	if (!task)
		return false;

	return cgroup_task_frozen(task) || cgroup_freezing(task);
}

extern void rekernel_report(int reporttype, int type, pid_t src_pid,
			    struct task_struct *src, pid_t dst_pid,
			    struct task_struct *dst, bool oneway,
			    const char *rpc_name, __u32 code);

#endif /* __NET_REKERNEL_H */
