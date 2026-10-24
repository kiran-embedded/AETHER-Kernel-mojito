/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * KernelSU (stock/tiann) — User-space API v6
 * Compatible with tiann/KernelSU manager application.
 *
 * UAPI Version history:
 *   v1: initial root grant
 *   v2: profile flags
 *   v3: scoped su-session fd
 *   v4: KSU_GET_INFO_FLAG_BUNDLED
 *   v5: EVENT_SERVICES start/skip result
 *   v6: KSU_IOCTL_LOOKUP_SYMBOLS + KSU_GET_INFO_FLAG_PREINIT
 *
 * NOTE: This is the stock KernelSU variant.
 *       KernelSU-Next lives in KernelSU-Next/uapi/
 */
#ifndef __KSU_STOCK_UAPI_H
#define __KSU_STOCK_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

/* ------------------------------------------------------------------ */
/*  UAPI version reported to the manager — MUST stay at 6             */
/* ------------------------------------------------------------------ */
#define KSU_STOCK_UAPI_VERSION   6

/* Magic numbers for the reboot-hook fd-install channel */
#define KSU_INSTALL_MAGIC1  0xDEADBEEF
#define KSU_INSTALL_MAGIC2  0xCAFEBABE

/* ------------------------------------------------------------------ */
/*  ksu_get_info flags                                                 */
/* ------------------------------------------------------------------ */
#define KSU_GET_INFO_FLAG_LKM      (1U << 0)  /* loaded as LKM        */
#define KSU_GET_INFO_FLAG_MANAGER  (1U << 1)  /* caller is manager    */
#define KSU_GET_INFO_FLAG_LATE     (1U << 2)  /* late-loaded          */
#define KSU_GET_INFO_FLAG_PR_BUILD (1U << 3)  /* production build     */
#define KSU_GET_INFO_FLAG_BUNDLED  (1U << 4)  /* bundled modules      */
#define KSU_GET_INFO_FLAG_PREINIT  (1U << 5)  /* preinit supported    */

/* ------------------------------------------------------------------ */
/*  Boot events                                                        */
/* ------------------------------------------------------------------ */
#define KSU_EVENT_POST_FS_DATA   1
#define KSU_EVENT_BOOT_COMPLETED 2
#define KSU_EVENT_MODULE_MOUNTED 3
#define KSU_EVENT_SERVICES       4

/* ------------------------------------------------------------------ */
/*  IOCTL command structs                                              */
/* ------------------------------------------------------------------ */

struct ksu_stock_get_info_cmd {
	__u32 version;       /* out: KERNEL_SU_VERSION */
	__u32 flags;         /* out: KSU_GET_INFO_FLAG_* */
	__u32 features;      /* out: max supported feature id */
	__u32 uapi_version;  /* out: KSU_STOCK_UAPI_VERSION (6) */
};

struct ksu_stock_uid_granted_root_cmd {
	__u32 uid;     /* in:  target UID */
	__u8  granted; /* out: 1 if root-granted */
};

struct ksu_stock_uid_should_umount_cmd {
	__u32 uid;          /* in:  target UID */
	__u8  should_umount; /* out: 1 if should umount */
};

struct ksu_stock_get_allow_list_cmd {
	__u32 uids[128]; /* out: UID array */
	__u32 count;     /* out: entry count */
	__u8  allow;     /* in:  1=allowlist, 0=denylist */
};

struct ksu_stock_new_get_allow_list_cmd {
	__u16 count;       /* in/out: requested/returned count */
	__u16 total_count; /* out: total entries */
	__u32 uids[0];     /* out: UID array (variable length) */
};

struct ksu_stock_report_event_cmd {
	__u32 event; /* in: KSU_EVENT_* */
};

struct ksu_stock_set_sepolicy_cmd {
	__u64          data_len; /* in: payload length */
	__aligned_u64  data;     /* in: user-space payload pointer */
};

struct ksu_stock_check_safemode_cmd {
	__u8 in_safe_mode; /* out: 1 if safe mode */
};

struct ksu_stock_app_profile {
	__u32 uid;
	__u32 flags;
	char  se_domain[64];  /* SELinux domain override */
	char  se_context[64]; /* SELinux context override */
};

struct ksu_stock_get_app_profile_cmd {
	struct ksu_stock_app_profile profile; /* in/out */
};

struct ksu_stock_set_app_profile_cmd {
	struct ksu_stock_app_profile profile; /* in */
};

struct ksu_stock_become_manager_cmd {
	__u8 token[65]; /* in: manager token (NUL-terminated) */
};

struct ksu_stock_get_manager_appid_cmd {
	__u32 appid; /* out: manager app-id */
};

struct ksu_stock_lookup_symbols_cmd {
	__aligned_u64 names; /* in:  ptr to 'count' char-ptr user pointers */
	__aligned_u64 addrs; /* out: ptr to 'count' __u64 addresses */
	__u32 count;         /* in:  symbol count (max KSU_STOCK_LOOKUP_MAX) */
	__u32 flags;         /* in:  reserved, must be 0 */
};
#define KSU_STOCK_LOOKUP_MAX  1024

/* ------------------------------------------------------------------ */
/*  IOCTL definitions — char 'K', same as stock KernelSU manager      */
/* ------------------------------------------------------------------ */
#define KSU_STOCK_IOCTL_GRANT_ROOT       _IOC(_IOC_NONE, 'K', 1, 0)
#define KSU_STOCK_IOCTL_GET_INFO         _IOR('K', 2, struct ksu_stock_get_info_cmd)
/* legacy: no size field */
#define KSU_STOCK_IOCTL_GET_INFO_LEGACY  _IOC(_IOC_READ, 'K', 2, 0)
#define KSU_STOCK_IOCTL_REPORT_EVENT     _IOC(_IOC_WRITE, 'K', 3, 0)
#define KSU_STOCK_IOCTL_SET_SEPOLICY     _IOC(_IOC_READ|_IOC_WRITE, 'K', 4, 0)
#define KSU_STOCK_IOCTL_CHECK_SAFEMODE   _IOC(_IOC_READ, 'K', 5, 0)
/* deprecated v1 allow/deny list */
#define KSU_STOCK_IOCTL_GET_ALLOW_LIST   _IOC(_IOC_READ|_IOC_WRITE, 'K', 6, 0)
#define KSU_STOCK_IOCTL_GET_DENY_LIST    _IOC(_IOC_READ|_IOC_WRITE, 'K', 7, 0)
/* v2 allow/deny list */
#define KSU_STOCK_IOCTL_NEW_GET_ALLOW    _IOWR('K', 6, struct ksu_stock_new_get_allow_list_cmd)
#define KSU_STOCK_IOCTL_NEW_GET_DENY     _IOWR('K', 7, struct ksu_stock_new_get_allow_list_cmd)
#define KSU_STOCK_IOCTL_UID_GRANTED_ROOT _IOC(_IOC_READ|_IOC_WRITE, 'K', 8, 0)
#define KSU_STOCK_IOCTL_UID_SHOULD_UMOUNT _IOC(_IOC_READ|_IOC_WRITE, 'K', 9, 0)
#define KSU_STOCK_IOCTL_GET_MANAGER_APPID _IOC(_IOC_READ, 'K', 10, 0)
#define KSU_STOCK_IOCTL_GET_APP_PROFILE   _IOC(_IOC_READ|_IOC_WRITE, 'K', 11, 0)
#define KSU_STOCK_IOCTL_SET_APP_PROFILE   _IOC(_IOC_WRITE, 'K', 12, 0)
/* v6: symbol batch lookup */
#define KSU_STOCK_IOCTL_LOOKUP_SYMBOLS    _IOW('K', 22, struct ksu_stock_lookup_symbols_cmd)

#endif /* __KSU_STOCK_UAPI_H */
