#ifndef __KSU_H_KSU_MANAGER
#define __KSU_H_KSU_MANAGER

#include <linux/cred.h>
#include <linux/types.h>

#define KSU_INVALID_APPID -1
#define KSU_PER_USER_RANGE 100000

#ifdef CONFIG_KSU_DISABLE_MANAGER
static inline bool ksu_is_manager_appid_valid()
{
    return true;
}

static inline bool is_manager()
{
    return current_uid().val == 0;
}

static inline bool is_uid_manager(uid_t uid)
{
    return uid == 0;
}

static inline uid_t ksu_get_manager_appid()
{
    return 0;
}

static inline void ksu_set_manager_appid(uid_t appid)
{
    (void)appid;
}

static inline void ksu_invalidate_manager_uid()
{
}
#else
#define KSU_MAX_MANAGERS 4
extern uid_t ksu_manager_appids[KSU_MAX_MANAGERS];
extern int ksu_manager_count;
extern uid_t ksu_manager_appid;

static inline bool ksu_is_manager_appid_valid(void)
{
	return ksu_manager_count > 0 || ksu_manager_appid != (uid_t)KSU_INVALID_APPID;
}

static inline bool is_uid_manager(uid_t uid)
{
	int i;
	uid_t appid = uid % KSU_PER_USER_RANGE;
	for (i = 0; i < ksu_manager_count; i++) {
		if (ksu_manager_appids[i] == appid)
			return true;
	}
	return unlikely(ksu_manager_appid == appid);
}

static inline bool is_manager(void)
{
	return is_uid_manager(current_uid().val);
}

static inline uid_t ksu_get_manager_appid(void)
{
	if (ksu_manager_appid != (uid_t)KSU_INVALID_APPID)
		return ksu_manager_appid;
	if (ksu_manager_count > 0)
		return ksu_manager_appids[0];
	return (uid_t)KSU_INVALID_APPID;
}

static inline void ksu_set_manager_appid(uid_t appid)
{
	int i;
	if (appid == (uid_t)KSU_INVALID_APPID) {
		ksu_manager_count = 0;
		ksu_manager_appid = (uid_t)KSU_INVALID_APPID;
		return;
	}
	for (i = 0; i < ksu_manager_count; i++) {
		if (ksu_manager_appids[i] == appid)
			return;
	}
	if (ksu_manager_count < KSU_MAX_MANAGERS) {
		ksu_manager_appids[ksu_manager_count++] = appid;
	}
	ksu_manager_appid = appid;
}

static inline void ksu_invalidate_manager_uid(void)
{
	ksu_manager_count = 0;
	ksu_manager_appid = (uid_t)KSU_INVALID_APPID;
}
#endif

#endif
