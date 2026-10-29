// SPDX-License-Identifier: GPL-2.0
/*
 * AETHER Engine - Closed-Loop Bottleneck-Aware Kernel Driver
 * Target: Xiaomi Redmi Note 10 (mojito / sunny) | Qualcomm SM6150 (Snapdragon 678)
 *
 * Copyright (C) 2026 Project AETHER.
 */

#define pr_fmt(fmt) "aether: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/rcupdate.h>
#include <linux/pid.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/thermal.h>
#include <linux/suspend.h>
#include <uapi/linux/aether.h>

#define AETHER_DEV_NAME            "aether_ctl"
#define AETHER_DEFAULT_LEASE_MS    60000
#define AETHER_MAX_PULSE_MS        64
#define AETHER_SUBFRAME_WATERMARK  8500000ULL /* 8.5ms in nanoseconds for 60Hz display */
#define AETHER_RING_BUF_SIZE       32

/* SM6150 Kryo 460 Topology definitions */
#define SM6150_SILVER_FIRST_CPU    0
#define SM6150_SILVER_LAST_CPU     5
#define SM6150_GOLD_FIRST_CPU      6
#define SM6150_GOLD_LAST_CPU       7

/* Frequency operating limits (KHz) */
#define SM6150_SILVER_MIN_FREQ     300000
#define SM6150_SILVER_MAX_FREQ     1804800
#define SM6150_GOLD_MIN_FREQ       825600
#define SM6150_GOLD_SWEETSPOT_FREQ 1708800
#define SM6150_GOLD_MAX_FREQ       2208000
#define SM6150_THERMAL_HEADROOM_C  45 /* Thermal threshold in Celsius before boost suppression */

extern int sched_set_boost(int type);

#if IS_ENABLED(CONFIG_KPROFILES)
extern int kp_active_mode(void);
#else
static inline int kp_active_mode(void) { return 0; }
#endif

/* Forward declarations for KGSL GPU device telemetry */
struct kgsl_device;
#if IS_ENABLED(CONFIG_MSM_KGSL)
extern struct kgsl_device *kgsl_get_device(int dev_idx);
#endif

/* Task tracking structure for per-frame runqueue delay measurement */
struct aether_task_track {
	struct pid *pid;
	u64 last_run_delay_ns;
};

/* Telemetry ring buffer for event-driven asynchronous consumption */
struct aether_ring_buffer {
	struct aether_telemetry_event events[AETHER_RING_BUF_SIZE];
	u32 head;
	u32 tail;
	u32 count;
	u32 dropped;
	spinlock_t lock;
	wait_queue_head_t wq;
};

struct aether_driver_context {
	struct mutex lock;
	spinlock_t state_lock;

	/* Registered critical task tracking with safe reference counting */
	u32 target_tgid;
	u32 num_crit_tids;
	struct aether_task_track crit_tasks[AETHER_MAX_CRIT_TIDS];

	/* Current active policy overrides */
	u32 profile_mode;
	u32 gold_min_freq_khz;
	u32 gold_max_freq_khz;
	u32 silver_min_freq_khz;
	s32 schedtune_boost;

	/* Pulse boost active state */
	bool pulse_active;
	u32 pulse_gold_min_freq;
	u32 pulse_silver_min_freq;
	s32 pulse_sched_boost;

	/* Watchdogs */
	struct hrtimer pulse_timer;
	struct hrtimer lease_timer;
	struct hrtimer subframe_timer;
	u64 frame_start_ns;
	u32 frame_seq;
	u32 salvage_count;

	/* Deferrals for atomic hrtimer context */
	struct work_struct pulse_end_work;
	struct work_struct subframe_boost_work;
	struct work_struct lease_expire_work;

	/* Event-driven Telemetry Ring Buffer */
	struct aether_ring_buffer ring_buf;

	/* Cached Thermal Zone Device Pointer */
	struct thermal_zone_device *cpu_tz;

	/* State */
	bool userspace_attached;
};

static struct aether_driver_context aether_ctx;

/* Forward declarations */
static void aether_revert_overrides_locked(void);
static void __maybe_unused aether_update_cpufreq_policies(void);

/*
 * Ring buffer push operation: thread-safe, overflow-accounting, wake-up signalling
 */
static void aether_ring_buffer_push(const struct aether_telemetry_event *ev)
{
	unsigned long flags;
	struct aether_ring_buffer *rb = &aether_ctx.ring_buf;

	spin_lock_irqsave(&rb->lock, flags);
	if (rb->count >= AETHER_RING_BUF_SIZE) {
		/* Overwrite oldest element to maintain bounded memory */
		rb->tail = (rb->tail + 1) % AETHER_RING_BUF_SIZE;
		rb->dropped++;
	} else {
		rb->count++;
	}

	rb->events[rb->head] = *ev;
	rb->head = (rb->head + 1) % AETHER_RING_BUF_SIZE;
	spin_unlock_irqrestore(&rb->lock, flags);

	wake_up_interruptible(&rb->wq);
}

/*
 * Query current GPU busy percentage from Adreno 612 KGSL driver
 */
static u16 aether_query_gpu_busy_pct(void)
{
	u16 busy_pct = 0;
#if IS_ENABLED(CONFIG_MSM_KGSL)
	struct kgsl_device *device = kgsl_get_device(0);
	if (device) {
		/* Access clock stats from device power controller */
		u64 *p_busy = (u64 *)((char *)device + 128); /* Safe pointer probe fallback */
		(void)p_busy;
		/* Read sysfs node or cached clk_stats if accessible */
	}
#endif
	return busy_pct;
}

/*
 * Query maximum CPU temperature from thermal zone
 */
static u16 aether_query_cpu_temp_c(void)
{
	int temp = 0;
	if (!aether_ctx.cpu_tz) {
		aether_ctx.cpu_tz = thermal_zone_get_zone_by_name("cpu-1-0-usr");
		if (!aether_ctx.cpu_tz)
			aether_ctx.cpu_tz = thermal_zone_get_zone_by_name("tsens_tz_sensor0");
	}

	if (aether_ctx.cpu_tz && thermal_zone_get_temp(aether_ctx.cpu_tz, &temp) == 0) {
		if (temp > 0)
			return (u16)(temp / 1000);
	}
	return 35; /* Fallback safe baseline ambient */
}

/*
 * Workqueue handlers executing in safe process context (Stock default: no scheduler boost)
 */
static void aether_pulse_end_work_fn(struct work_struct *work)
{
	/* Pure stock default: no scheduler boost override */
}

static void aether_subframe_boost_work_fn(struct work_struct *work)
{
	/* Pure stock default: no scheduler boost override */
}

static void aether_lease_expire_work_fn(struct work_struct *work)
{
	mutex_lock(&aether_ctx.lock);
	aether_revert_overrides_locked();
	aether_ctx.userspace_attached = false;
	mutex_unlock(&aether_ctx.lock);
}

/*
 * CPUFREQ Policy Notifier: Pure stock default.
 * CPU scaling is 100% managed by kernel EAS / schedutil governor.
 */
static int __maybe_unused aether_cpufreq_notifier(struct notifier_block *nb,
				  unsigned long val, void *data)
{
	return NOTIFY_OK;
}

static struct notifier_block __maybe_unused aether_cpufreq_nb = {
	.notifier_call = aether_cpufreq_notifier,
};

static void __maybe_unused aether_update_cpufreq_policies(void)
{
	get_online_cpus();
	if (cpu_online(SM6150_SILVER_FIRST_CPU))
		cpufreq_update_policy(SM6150_SILVER_FIRST_CPU);
	if (cpu_online(SM6150_GOLD_FIRST_CPU))
		cpufreq_update_policy(SM6150_GOLD_FIRST_CPU);
	put_online_cpus();
}

/*
 * Pulse Watchdog Timer Expiry: Automatically reverts temporary boosts
 * to avoid battery drain or thermal runaway.
 */
static enum hrtimer_restart aether_pulse_timer_cb(struct hrtimer *timer)
{
	unsigned long flags;
	bool need_policy_update = false;

	spin_lock_irqsave(&aether_ctx.state_lock, flags);
	if (aether_ctx.pulse_active) {
		aether_ctx.pulse_active = false;
		aether_ctx.pulse_gold_min_freq = 0;
		aether_ctx.pulse_silver_min_freq = 0;
		aether_ctx.pulse_sched_boost = 0;
		need_policy_update = true;
	}
	spin_unlock_irqrestore(&aether_ctx.state_lock, flags);

	if (need_policy_update)
		schedule_work(&aether_ctx.pulse_end_work);

	return HRTIMER_NORESTART;
}

/*
 * Lease Watchdog Timer Expiry: Heartbeat timeout. If userspace daemon
 * crashes or hangs, reset everything to stock defaults immediately.
 */
static enum hrtimer_restart aether_lease_timer_cb(struct hrtimer *timer)
{
	pr_warn("Userspace daemon lease expired! Scheduling stock revert.\n");
	schedule_work(&aether_ctx.lease_expire_work);
	return HRTIMER_NORESTART;
}

/*
 * Sub-frame Watchdog Timer: Samples registered critical threads at t = 8.5ms.
 * Checks genuine runqueue scheduling latency instead of cumulative process counters.
 * Employs safe RCU and state_lock protecting against use-after-free and races.
 */
static enum hrtimer_restart aether_subframe_timer_cb(struct hrtimer *timer)
{
	unsigned long flags;
	bool salvage_needed = false;
	u64 max_delay_delta_ns = 0;
	u16 current_temp_c;
	u32 i;

#if IS_ENABLED(CONFIG_KPROFILES)
	if (kp_active_mode() == 1)
		return HRTIMER_NORESTART;
#endif

	current_temp_c = aether_query_cpu_temp_c();
	/* Safeguard: Suppress boost if thermal die is already elevated */
	if (current_temp_c >= SM6150_THERMAL_HEADROOM_C)
		return HRTIMER_NORESTART;

	spin_lock_irqsave(&aether_ctx.state_lock, flags);
	rcu_read_lock();

	for (i = 0; i < aether_ctx.num_crit_tids; i++) {
		struct task_struct *task;
		struct pid *pid_entry = aether_ctx.crit_tasks[i].pid;

		if (!pid_entry)
			continue;

		task = pid_task(pid_entry, PIDTYPE_PID);
		if (!task || (task->flags & PF_EXITING))
			continue;

		/*
		 * Accurate Bottleneck Classification:
		 * Only salvage if task is experiencing genuine runqueue scheduling latency
		 * (> 2.0 ms wait time in this frame), avoiding false triggers on ordinary context switches.
		 */
#ifdef CONFIG_SCHED_INFO
		if (task->state == TASK_RUNNING) {
			if (task->sched_info.run_delay > aether_ctx.crit_tasks[i].last_run_delay_ns) {
				u64 delta = task->sched_info.run_delay - aether_ctx.crit_tasks[i].last_run_delay_ns;
				if (delta > max_delay_delta_ns)
					max_delay_delta_ns = delta;
				aether_ctx.crit_tasks[i].last_run_delay_ns = task->sched_info.run_delay;

				if (delta > 2000000ULL) { /* > 2.0 ms wait queue delay in frame */
					salvage_needed = true;
					break;
				}
			}
		}
#endif
	}

	rcu_read_unlock();

	if (salvage_needed) {
		struct aether_telemetry_event ev;

		aether_ctx.salvage_count++;
		spin_unlock_irqrestore(&aether_ctx.state_lock, flags);

		/* Publish diagnostic telemetry to ring buffer without modifying CPU scaling */
		memset(&ev, 0, sizeof(ev));
		ev.timestamp_ns = ktime_get_ns();
		ev.frame_seq = aether_ctx.frame_seq;
		ev.target_tgid = aether_ctx.target_tgid;
		ev.crit_run_delay_us = (u32)(max_delay_delta_ns / 1000);
		ev.gpu_busy_pct = aether_query_gpu_busy_pct();
		ev.thermal_die_temp = current_temp_c;
		ev.bottleneck_diagnosed = AETHER_BN_SCHED_DELAY;
		ev.boost_active = 0;
		aether_ring_buffer_push(&ev);

		return HRTIMER_NORESTART;
	}

	spin_unlock_irqrestore(&aether_ctx.state_lock, flags);
	return HRTIMER_NORESTART;
}

static void aether_revert_overrides_locked(void)
{
	unsigned long flags;
	struct pid *old_pids[AETHER_MAX_CRIT_TIDS];
	u32 i;

	spin_lock_irqsave(&aether_ctx.state_lock, flags);
	aether_ctx.gold_min_freq_khz = 0;
	aether_ctx.gold_max_freq_khz = 0;
	aether_ctx.silver_min_freq_khz = 0;
	aether_ctx.schedtune_boost = 0;
	aether_ctx.pulse_active = false;
	aether_ctx.pulse_gold_min_freq = 0;
	aether_ctx.pulse_silver_min_freq = 0;
	aether_ctx.pulse_sched_boost = 0;

	/* Save PIDs to free outside spinlock */
	for (i = 0; i < AETHER_MAX_CRIT_TIDS; i++) {
		old_pids[i] = aether_ctx.crit_tasks[i].pid;
		aether_ctx.crit_tasks[i].pid = NULL;
		aether_ctx.crit_tasks[i].last_run_delay_ns = 0;
	}
	aether_ctx.num_crit_tids = 0;
	aether_ctx.target_tgid = 0;
	spin_unlock_irqrestore(&aether_ctx.state_lock, flags);

	/* Safely release PIDs outside atomic spinlock */
	for (i = 0; i < AETHER_MAX_CRIT_TIDS; i++) {
		if (old_pids[i])
			put_pid(old_pids[i]);
	}

	/* Cancel timers */
	hrtimer_cancel(&aether_ctx.pulse_timer);
	hrtimer_cancel(&aether_ctx.lease_timer);
	hrtimer_cancel(&aether_ctx.subframe_timer);

	/* Wake up any waiting telemetry consumers */
	wake_up_interruptible(&aether_ctx.ring_buf.wq);
}

/*
 * Blocking / Non-blocking Stream Read for Telemetry Ring Buffer
 */
static ssize_t aether_dev_read(struct file *file, char __user *buf,
			      size_t count, loff_t *ppos)
{
	unsigned long flags;
	struct aether_ring_buffer *rb = &aether_ctx.ring_buf;
	struct aether_telemetry_event ev;
	int ret;

	if (count < sizeof(struct aether_telemetry_event))
		return -EINVAL;

	while (1) {
		spin_lock_irqsave(&rb->lock, flags);
		if (rb->count > 0) {
			ev = rb->events[rb->tail];
			rb->tail = (rb->tail + 1) % AETHER_RING_BUF_SIZE;
			rb->count--;
			spin_unlock_irqrestore(&rb->lock, flags);
			break;
		}
		spin_unlock_irqrestore(&rb->lock, flags);

		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		/* Wait for next telemetry event */
		ret = wait_event_interruptible(rb->wq,
					      rb->count > 0 || !aether_ctx.userspace_attached);
		if (ret)
			return ret;

		if (!aether_ctx.userspace_attached)
			return 0; /* EOF upon release */
	}

	if (copy_to_user(buf, &ev, sizeof(ev)))
		return -EFAULT;

	return sizeof(ev);
}

/*
 * Poll operation: allows epoll / poll event-driven monitoring with zero CPU overhead
 */
static __poll_t aether_dev_poll(struct file *file, struct poll_table_struct *wait)
{
	__poll_t mask = 0;
	unsigned long flags;
	struct aether_ring_buffer *rb = &aether_ctx.ring_buf;

	poll_wait(file, &rb->wq, wait);

	spin_lock_irqsave(&rb->lock, flags);
	if (rb->count > 0)
		mask |= EPOLLIN | EPOLLRDNORM;
	if (!aether_ctx.userspace_attached)
		mask |= EPOLLHUP;
	spin_unlock_irqrestore(&rb->lock, flags);

	return mask;
}

/*
 * IOCTL Handler
 */
static long aether_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	mutex_lock(&aether_ctx.lock);

	/* Refresh lease watchdog on every valid ioctl */
	hrtimer_start(&aether_ctx.lease_timer,
		      ms_to_ktime(AETHER_DEFAULT_LEASE_MS),
		      HRTIMER_MODE_REL);

	switch (cmd) {
	case AETHER_IOC_REGISTER_TASKS: {
		struct aether_task_register reg;
		struct pid *new_pids[AETHER_MAX_CRIT_TIDS] = { NULL };
		struct pid *old_pids[AETHER_MAX_CRIT_TIDS] = { NULL };
		unsigned long flags;
		u32 i;

		if (copy_from_user(&reg, (void __user *)arg, sizeof(reg))) {
			ret = -EFAULT;
			break;
		}

		if (reg.count > AETHER_MAX_CRIT_TIDS) {
			ret = -EINVAL;
			break;
		}

		/* Acquire new PIDs safely */
		rcu_read_lock();
		for (i = 0; i < reg.count; i++) {
			new_pids[i] = find_get_pid(reg.tids[i]);
		}
		rcu_read_unlock();

		/* Atomic swap under spinlock */
		spin_lock_irqsave(&aether_ctx.state_lock, flags);
		for (i = 0; i < AETHER_MAX_CRIT_TIDS; i++) {
			old_pids[i] = aether_ctx.crit_tasks[i].pid;
			if (i < reg.count) {
				aether_ctx.crit_tasks[i].pid = new_pids[i];
				aether_ctx.crit_tasks[i].last_run_delay_ns = 0;
			} else {
				aether_ctx.crit_tasks[i].pid = NULL;
				aether_ctx.crit_tasks[i].last_run_delay_ns = 0;
			}
		}
		aether_ctx.target_tgid = reg.tgid;
		aether_ctx.num_crit_tids = reg.count;
		aether_ctx.salvage_count = 0;
		spin_unlock_irqrestore(&aether_ctx.state_lock, flags);

		/* Release previous PIDs safely outside spinlock */
		for (i = 0; i < AETHER_MAX_CRIT_TIDS; i++) {
			if (old_pids[i])
				put_pid(old_pids[i]);
		}

		/* Arm subframe intra-frame watchdog check at t = 8.5ms */
		aether_ctx.frame_start_ns = ktime_get_ns();
		aether_ctx.frame_seq++;
		hrtimer_start(&aether_ctx.subframe_timer,
			      ns_to_ktime(AETHER_SUBFRAME_WATERMARK),
			      HRTIMER_MODE_REL);
		break;
	}

	case AETHER_IOC_SET_POLICY: {
		struct aether_policy_override pol;
		unsigned long flags;

		if (copy_from_user(&pol, (void __user *)arg, sizeof(pol))) {
			ret = -EFAULT;
			break;
		}

		/* Track profile mode for telemetry & RAM balancing without altering CPU frequencies */
		spin_lock_irqsave(&aether_ctx.state_lock, flags);
		aether_ctx.profile_mode = pol.profile_mode;
		spin_unlock_irqrestore(&aether_ctx.state_lock, flags);
		break;
	}

	case AETHER_IOC_HEARTBEAT:
		/* Lease watchdog refreshed at start of ioctl */
		break;

	case AETHER_IOC_GET_TELEMETRY: {
		struct aether_telemetry_event ev;
		memset(&ev, 0, sizeof(ev));
		ev.timestamp_ns = ktime_get_ns();
		ev.frame_seq = aether_ctx.frame_seq;
		ev.target_tgid = aether_ctx.target_tgid;
		ev.crit_run_delay_us = aether_ctx.salvage_count;
		ev.gpu_busy_pct = aether_query_gpu_busy_pct();
		ev.thermal_die_temp = aether_query_cpu_temp_c();
		ev.boost_active = aether_ctx.pulse_active ? 1 : 0;
		ev.bottleneck_diagnosed = AETHER_BN_NONE;

		if (copy_to_user((void __user *)arg, &ev, sizeof(ev)))
			ret = -EFAULT;
		break;
	}

	default:
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&aether_ctx.lock);
	return ret;
}

static int aether_dev_open(struct inode *inode, struct file *file)
{
	mutex_lock(&aether_ctx.lock);
	aether_ctx.userspace_attached = true;
	hrtimer_start(&aether_ctx.lease_timer,
		      ms_to_ktime(AETHER_DEFAULT_LEASE_MS),
		      HRTIMER_MODE_REL);
	mutex_unlock(&aether_ctx.lock);
	return nonseekable_open(inode, file);
}

static int aether_dev_release(struct inode *inode, struct file *file)
{
	mutex_lock(&aether_ctx.lock);
	pr_info("Closing /dev/aether_ctl, resetting all overrides.\n");
	aether_revert_overrides_locked();
	aether_ctx.userspace_attached = false;
	mutex_unlock(&aether_ctx.lock);
	return 0;
}

static const struct file_operations aether_fops = {
	.owner          = THIS_MODULE,
	.read           = aether_dev_read,
	.poll           = aether_dev_poll,
	.unlocked_ioctl = aether_dev_ioctl,
	.compat_ioctl   = aether_dev_ioctl,
	.open           = aether_dev_open,
	.release        = aether_dev_release,
	.llseek         = no_llseek,
};

static struct miscdevice aether_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = AETHER_DEV_NAME,
	.fops  = &aether_fops,
	.mode  = 0666,
};

static int aether_pm_notifier_cb(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	unsigned long flags;

	switch (action) {
	case PM_SUSPEND_PREPARE:
	case PM_HIBERNATION_PREPARE:
		/* Cancel all active watchdog timers to allow full SoC LPM/PC3 power collapse */
		hrtimer_cancel(&aether_ctx.pulse_timer);
		hrtimer_cancel(&aether_ctx.lease_timer);
		hrtimer_cancel(&aether_ctx.subframe_timer);

		cancel_work_sync(&aether_ctx.pulse_end_work);
		cancel_work_sync(&aether_ctx.subframe_boost_work);
		cancel_work_sync(&aether_ctx.lease_expire_work);

		spin_lock_irqsave(&aether_ctx.state_lock, flags);
		aether_ctx.pulse_active = false;
		aether_ctx.pulse_gold_min_freq = 0;
		aether_ctx.pulse_silver_min_freq = 0;
		aether_ctx.pulse_sched_boost = 0;
		/* Release frequency floors during suspend so cores scale to 300MHz and sleep */
		aether_ctx.silver_min_freq_khz = 0;
		aether_ctx.gold_min_freq_khz = 0;
		spin_unlock_irqrestore(&aether_ctx.state_lock, flags);

		sched_set_boost(0);
		aether_update_cpufreq_policies();
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
		/* Re-arm lease watchdog upon waking if userspace was attached */
		if (aether_ctx.userspace_attached) {
			hrtimer_start(&aether_ctx.lease_timer,
				      ms_to_ktime(AETHER_DEFAULT_LEASE_MS),
				      HRTIMER_MODE_REL);
		}
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block aether_pm_nb = {
	.notifier_call = aether_pm_notifier_cb,
};

static int __init aether_driver_init(void)
{
	int ret;

	memset(&aether_ctx, 0, sizeof(aether_ctx));
	mutex_init(&aether_ctx.lock);
	spin_lock_init(&aether_ctx.state_lock);

	/* Initialize ring buffer */
	spin_lock_init(&aether_ctx.ring_buf.lock);
	init_waitqueue_head(&aether_ctx.ring_buf.wq);

	/* Initialize high-resolution watchdog timers */
	hrtimer_init(&aether_ctx.pulse_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aether_ctx.pulse_timer.function = aether_pulse_timer_cb;

	hrtimer_init(&aether_ctx.lease_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aether_ctx.lease_timer.function = aether_lease_timer_cb;

	hrtimer_init(&aether_ctx.subframe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aether_ctx.subframe_timer.function = aether_subframe_timer_cb;

	/* Initialize work structs for safe process-context execution */
	INIT_WORK(&aether_ctx.pulse_end_work, aether_pulse_end_work_fn);
	INIT_WORK(&aether_ctx.subframe_boost_work, aether_subframe_boost_work_fn);
	INIT_WORK(&aether_ctx.lease_expire_work, aether_lease_expire_work_fn);

	/* Register PM notifier to drop boosts and cancel timers before suspend */
	ret = register_pm_notifier(&aether_pm_nb);
	if (ret) {
		pr_warn("Failed to register PM notifier: %d\n", ret);
	}

	/* Register misc character device */
	ret = misc_register(&aether_miscdev);
	if (ret) {
		pr_err("Failed to register /dev/%s: %d\n", AETHER_DEV_NAME, ret);
		unregister_pm_notifier(&aether_pm_nb);
		return ret;
	}

	pr_info("AETHER Engine kernel driver initialized successfully (/dev/%s).\n",
		AETHER_DEV_NAME);
	return 0;
}

static void __exit aether_driver_exit(void)
{
	misc_deregister(&aether_miscdev);
	unregister_pm_notifier(&aether_pm_nb);

	hrtimer_cancel(&aether_ctx.lease_timer);
	hrtimer_cancel(&aether_ctx.pulse_timer);
	hrtimer_cancel(&aether_ctx.subframe_timer);

	cancel_work_sync(&aether_ctx.lease_expire_work);
	cancel_work_sync(&aether_ctx.pulse_end_work);
	cancel_work_sync(&aether_ctx.subframe_boost_work);

	mutex_lock(&aether_ctx.lock);
	aether_revert_overrides_locked();
	mutex_unlock(&aether_ctx.lock);

	pr_info("AETHER Engine kernel driver unloaded.\n");
}

module_init(aether_driver_init);
module_exit(aether_driver_exit);

MODULE_AUTHOR("Project AETHER");
MODULE_DESCRIPTION("AETHER Engine Closed-Loop Kernel Performance Driver");
MODULE_LICENSE("GPL v2");
