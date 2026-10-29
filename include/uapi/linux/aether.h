/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * AETHER Engine - Closed-Loop Bottleneck-Aware Kernel Performance Architecture
 * Target: Xiaomi Redmi Note 10 (mojito / sunny) | Qualcomm SM6150 (Snapdragon 678)
 *
 * Copyright (C) 2026 Project AETHER.
 */

#ifndef _UAPI_LINUX_AETHER_H
#define _UAPI_LINUX_AETHER_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define AETHER_MAGIC            'A'
#define AETHER_ABI_VERSION      1
#define AETHER_MAX_CRIT_TIDS    8

/* Bottleneck classification states */
enum aether_bottleneck_type {
	AETHER_BN_NONE          = 0,
	AETHER_BN_CPU_COMPUTE   = 1,
	AETHER_BN_SCHED_DELAY   = 2,
	AETHER_BN_GPU_BOUND     = 3,
	AETHER_BN_MEM_THRASH    = 4,
	AETHER_BN_THERMAL_CLIP  = 5,
};

/* High-level profile hints passed from userspace / APK */
enum aether_profile_mode {
	AETHER_PROFILE_DEFAULT     = 0,
	AETHER_PROFILE_BATTERY     = 1,
	AETHER_PROFILE_BALANCED    = 2,
	AETHER_PROFILE_GAMING      = 3,
	AETHER_PROFILE_MEDIA       = 4,
	AETHER_PROFILE_PERFORMANCE = 5,
};

/* Task registration structure (UI Thread, RenderThread, UnityMain, etc.) */
struct aether_task_register {
	__u32 tgid;                         /* Foreground App TGID / PID */
	__u32 count;                        /* Number of critical threads */
	__u32 tids[AETHER_MAX_CRIT_TIDS];   /* Critical rendering TIDs */
};

/* Policy override parameters */
struct aether_policy_override {
	__u32 profile_mode;                 /* enum aether_profile_mode */
	__u32 gold_min_freq_khz;            /* Min frequency floor for Kryo 460 Gold (Policy 6) */
	__u32 gold_max_freq_khz;            /* Max frequency cap for Kryo 460 Gold (0 = uncapped) */
	__u32 silver_min_freq_khz;          /* Min frequency floor for Kryo 460 Silver (Policy 0) */
	__s32 schedtune_boost;              /* Schedtune boost override (-100 to 100, 0 = default) */
	__u32 pulse_duration_ms;            /* Duration of boost pulse (max 64ms, failsafe) */
};

/* Real-time telemetry snapshot */
struct aether_telemetry_event {
	__u64 timestamp_ns;                 /* Monotonic kernel timestamp */
	__u32 frame_seq;                    /* Monotonic frame sequence counter */
	__u32 target_tgid;                  /* Foreground process PID */
	__u32 crit_runtime_us;              /* Active execution runtime */
	__u32 crit_run_delay_us;            /* Cumulative scheduler runqueue delay */
	__u16 gpu_busy_pct;                 /* Adreno 612 busy percentage (0 - 100) */
	__u16 mem_stall_pct;                /* Memory stall indicator */
	__u16 thermal_die_temp;             /* Maximum CPU/GPU temperature in Celsius */
	__u8  bottleneck_diagnosed;         /* enum aether_bottleneck_type */
	__u8  boost_active;                 /* 1 if salvage pulse active, 0 otherwise */
};

/* IOCTL Commands */
#define AETHER_IOC_REGISTER_TASKS  _IOW(AETHER_MAGIC, 1, struct aether_task_register)
#define AETHER_IOC_SET_POLICY      _IOW(AETHER_MAGIC, 2, struct aether_policy_override)
#define AETHER_IOC_HEARTBEAT       _IO(AETHER_MAGIC, 3)
#define AETHER_IOC_GET_TELEMETRY   _IOR(AETHER_MAGIC, 4, struct aether_telemetry_event)

#endif /* _UAPI_LINUX_AETHER_H */
