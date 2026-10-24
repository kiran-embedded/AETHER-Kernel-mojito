#ifndef __KSU_H_MANAGER_IDENTITY
#define __KSU_H_MANAGER_IDENTITY

// #include "allowlist.h"

#define KSU_INVALID_APPID -1
#define KSU_PER_USER_RANGE 100000
#define KSU_MAX_MANAGERS 4

extern uid_t ksu_manager_appids[KSU_MAX_MANAGERS];
#define ksu_manager_appid (ksu_manager_appids[0])

static inline bool ksu_is_manager_appid_valid(void)
{
	int i;
	for (i = 0; i < KSU_MAX_MANAGERS; i++) {
		if (ksu_manager_appids[i] != KSU_INVALID_APPID)
			return true;
	}
	return false;
}

static inline bool is_manager(void)
{
	uid_t appid = current_uid().val % KSU_PER_USER_RANGE;
	int i;
	for (i = 0; i < KSU_MAX_MANAGERS; i++) {
		if (ksu_manager_appids[i] != KSU_INVALID_APPID && ksu_manager_appids[i] == appid)
			return true;
	}
	return false;
}

static inline bool is_uid_manager(uid_t uid)
{
	uid_t appid = uid % KSU_PER_USER_RANGE;
	int i;
	for (i = 0; i < KSU_MAX_MANAGERS; i++) {
		if (ksu_manager_appids[i] != KSU_INVALID_APPID && ksu_manager_appids[i] == appid)
			return true;
	}
	return false;
}

static inline uid_t ksu_get_manager_appid(void)
{
	uid_t appid = current_uid().val % KSU_PER_USER_RANGE;
	int i;
	for (i = 0; i < KSU_MAX_MANAGERS; i++) {
		if (ksu_manager_appids[i] != KSU_INVALID_APPID && ksu_manager_appids[i] == appid)
			return appid;
	}
	for (i = 0; i < KSU_MAX_MANAGERS; i++) {
		if (ksu_manager_appids[i] != KSU_INVALID_APPID)
			return ksu_manager_appids[i];
	}
	return KSU_INVALID_APPID;
}

static inline void ksu_set_manager_appid(uid_t appid)
{
	int i;
	if (appid == KSU_INVALID_APPID)
		return;
	for (i = 0; i < KSU_MAX_MANAGERS; i++) {
		if (ksu_manager_appids[i] == appid)
			return;
	}
	for (i = 0; i < KSU_MAX_MANAGERS; i++) {
		if (ksu_manager_appids[i] == KSU_INVALID_APPID) {
			ksu_manager_appids[i] = appid;
			return;
		}
	}
	ksu_manager_appids[KSU_MAX_MANAGERS - 1] = appid;
}

static inline void ksu_invalidate_manager_uid(void)
{
	int i;
	for (i = 0; i < KSU_MAX_MANAGERS; i++)
		ksu_manager_appids[i] = KSU_INVALID_APPID;
}

#endif
