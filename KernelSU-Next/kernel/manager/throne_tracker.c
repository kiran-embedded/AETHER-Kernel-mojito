#include "ksu.h"
#include "linux/cred.h"
#include "linux/namei.h"
#include "util.h"
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/version.h>

#include "policy/allowlist.h"
#include "manager/apk_sign.h"
#include "klog.h" // IWYU pragma: keep
#include "manager/manager_identity.h"
#include "manager/throne_tracker.h"

static DEFINE_MUTEX(throne_tracker_mutex);

uid_t ksu_manager_appids[KSU_MAX_MANAGERS] = { (uid_t)KSU_INVALID_APPID };
int ksu_manager_count = 0;
uid_t ksu_manager_appid = (uid_t)KSU_INVALID_APPID;

#define DATA_PATH_LEN 384 // 384 is enough for /data/app/<package>/base.apk

static char cached_manager_apk_path[DATA_PATH_LEN];
static char manager_pkg[KSU_MAX_PACKAGE_NAME];

struct ksu_manager_entry {
	uid_t uid;
	char package[KSU_MAX_PACKAGE_NAME];
};

static struct ksu_manager_entry ksu_managers[KSU_MAX_MANAGERS];
static int ksu_crowned_count = 0;

const char *ksu_get_manager_pkg_by_uid(uid_t uid)
{
	int i;
	uid_t appid = uid % KSU_PER_USER_RANGE;
	for (i = 0; i < ksu_crowned_count; i++) {
		if (ksu_managers[i].uid % KSU_PER_USER_RANGE == appid)
			return ksu_managers[i].package;
	}
	if (ksu_crowned_count > 0)
		return ksu_managers[0].package;
	return NULL;
}

const char *ksu_get_manager_pkg(void)
{
	return manager_pkg;
}

#define SYSTEM_PACKAGES_LIST_PATH "/data/system/packages.list"

struct uid_data {
	struct list_head list;
	u32 uid;
	char package[KSU_MAX_PACKAGE_NAME];
};

static void crown_manager(const char *apk, struct list_head *uid_data)
{
	struct list_head *list = (struct list_head *)uid_data;
	struct uid_data *np;
	const char *target_pkg;

#ifdef KSU_MANAGER_PACKAGE
	target_pkg = KSU_MANAGER_PACKAGE;
#else
	char pkg[KSU_MAX_PACKAGE_NAME];
	char dir_path[DATA_PATH_LEN];
	char *last_slash;

	strscpy(dir_path, apk, sizeof(dir_path));
	last_slash = strrchr(dir_path, '/');
	if (last_slash)
		*last_slash = '\0'; // strip /base.apk

	if (get_pkg_from_apk_dir_path(pkg, dir_path) < 0) {
		pr_err("Failed to get package name from apk dir path: %s\n", dir_path);
		return;
	}
	pr_info("manager pkg: %s\n", pkg);
	target_pkg = pkg;
#endif

	list_for_each_entry (np, list, list) {
		if (strncmp(np->package, target_pkg, KSU_MAX_PACKAGE_NAME) == 0) {
			int i;
			bool already_crowned = false;
			for (i = 0; i < ksu_crowned_count; i++) {
				if (ksu_managers[i].uid == np->uid) {
					already_crowned = true;
					break;
				}
			}
			if (!already_crowned && ksu_crowned_count < KSU_MAX_MANAGERS) {
				ksu_managers[ksu_crowned_count].uid = np->uid;
				strscpy(ksu_managers[ksu_crowned_count].package, target_pkg, KSU_MAX_PACKAGE_NAME);
				ksu_crowned_count++;
			}
			pr_info("Crowning manager: %s(uid=%d)\n", target_pkg, np->uid);
			strscpy(manager_pkg, target_pkg, sizeof(manager_pkg));
			ksu_set_manager_appid(np->uid);
			break;
		}
	}
}

struct data_path {
	char dirpath[DATA_PATH_LEN];
	int depth;
	struct list_head list;
};

struct apk_path_hash {
	unsigned int hash;
	bool exists;
	struct list_head list;
};

static struct list_head apk_path_hash_list = LIST_HEAD_INIT(apk_path_hash_list);

struct my_dir_context {
	struct dir_context ctx;
	struct list_head *data_path_list;
	char *parent_dir;
	void *private_data;
	int depth;
};

// https://docs.kernel.org/filesystems/porting.html
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#define FILLDIR_RETURN_TYPE bool
#define FILLDIR_ACTOR_CONTINUE true
#define FILLDIR_ACTOR_STOP false
#else
#define FILLDIR_RETURN_TYPE int
#define FILLDIR_ACTOR_CONTINUE 0
#define FILLDIR_ACTOR_STOP -EINVAL
#endif

extern bool is_manager_apk(char *path);

static bool maybe_manager_apk_dir(const char *path)
{
#ifdef KSU_MANAGER_PACKAGE
	_Static_assert(sizeof(KSU_MANAGER_PACKAGE) < KSU_MAX_PACKAGE_NAME, "KSU_MANAGER_PACKAGE too long!");
	char pkg[KSU_MAX_PACKAGE_NAME];
	if (get_pkg_from_apk_dir_path(pkg, path) < 0) {
		pr_err("Failed to get package name from apk dir path: %s\n", path);
		return false;
	}

	// pkg is `<real package>`
	return strncmp(pkg, KSU_MANAGER_PACKAGE, sizeof(KSU_MANAGER_PACKAGE)) == 0;
#else
	return true;
#endif
}

FILLDIR_RETURN_TYPE my_actor(struct dir_context *ctx, const char *name,
							int namelen, loff_t off, u64 ino,
							unsigned int d_type)
{
	struct my_dir_context *my_ctx =
		container_of(ctx, struct my_dir_context, ctx);
	char dirpath[DATA_PATH_LEN];

	if (!strncmp(name, "..", namelen) || !strncmp(name, ".", namelen))
		return FILLDIR_ACTOR_CONTINUE; // Skip "." and ".."

	if (d_type == DT_DIR && namelen >= 8 && !strncmp(name, "vmdl", 4) &&
		!strncmp(name + namelen - 4, ".tmp", 4)) {
		pr_info("Skipping directory: %.*s\n", namelen, name);
		return FILLDIR_ACTOR_CONTINUE; // Skip staging package
	}

	if (snprintf(dirpath, DATA_PATH_LEN, "%s/%.*s", my_ctx->parent_dir, namelen,
				name) >= DATA_PATH_LEN) {
		pr_err("Path too long: %s/%.*s\n", my_ctx->parent_dir, namelen, name);
		return FILLDIR_ACTOR_CONTINUE;
	}

	if (d_type == DT_DIR && my_ctx->depth > 0) {
		struct data_path *data = kzalloc(sizeof(struct data_path), GFP_KERNEL);

		if (!data) {
			pr_err("Failed to allocate memory for %s\n", dirpath);
			return FILLDIR_ACTOR_CONTINUE;
		}

		strscpy(data->dirpath, dirpath, DATA_PATH_LEN);
		data->depth = my_ctx->depth - 1;
		list_add_tail(&data->list, my_ctx->data_path_list);
	} else {
		if ((namelen == 8) && (strncmp(name, "base.apk", namelen) == 0)) {
			if (maybe_manager_apk_dir(my_ctx->parent_dir)) {
				snprintf((char *)my_ctx->private_data, DATA_PATH_LEN, "%s/%.*s", my_ctx->parent_dir, namelen, name);
				strcpy(cached_manager_apk_path, (char *)my_ctx->private_data);
			}
			return FILLDIR_ACTOR_STOP;
		}
	}

	return FILLDIR_ACTOR_CONTINUE;
}

void search_manager(const char *path, int depth, struct list_head *uid_data)
{
	int i, stop = 0;
	struct list_head data_path_list;
	INIT_LIST_HEAD(&data_path_list);
	unsigned long data_app_magic = 0;

	// Initialize APK cache list
	struct apk_path_hash *pos, *n;
	list_for_each_entry (pos, &apk_path_hash_list, list) {
		pos->exists = false;
	}

	// First depth
	struct data_path data;
	strscpy(data.dirpath, path, DATA_PATH_LEN);
	data.depth = depth;
	list_add_tail(&data.list, &data_path_list);

	char candidate_path[DATA_PATH_LEN];
	candidate_path[0] = 0;

	for (i = depth; i >= 0; i--) {
		struct data_path *pos, *n;

		list_for_each_entry_safe (pos, n, &data_path_list, list) {
			if (stop)
				goto skip_iterate;

			struct my_dir_context ctx = { .ctx.actor = my_actor,
										.data_path_list = &data_path_list,
										.parent_dir = pos->dirpath,
										.private_data = candidate_path,
										.depth = pos->depth };
			struct file *file = ksu_filp_open_nonotify(pos->dirpath, O_RDONLY | O_NOFOLLOW | O_NOATIME);
			if (IS_ERR(file)) {
				pr_err("Failed to open directory: %s, err: %ld\n",
					pos->dirpath, PTR_ERR(file));
				goto skip_iterate;
			}

			// grab magic on first folder, which is /data/app
			if (!data_app_magic) {
				if (file->f_inode->i_sb->s_magic) {
					data_app_magic = file->f_inode->i_sb->s_magic;
					pr_info("%s: dir: %s got magic! 0x%lx\n", __func__,
							pos->dirpath, data_app_magic);
				} else {
					filp_close(file, NULL);
					goto skip_iterate;
				}
			}

			if (file->f_inode->i_sb->s_magic != data_app_magic) {
				pr_info("%s: skip: %s magic: 0x%lx expected: 0x%lx\n",
						__func__, pos->dirpath,
						file->f_inode->i_sb->s_magic, data_app_magic);
				filp_close(file, NULL);
				goto skip_iterate;
			}

			candidate_path[0] = 0;
			iterate_dir(file, &ctx.ctx);
			filp_close(file, NULL);

			if (!candidate_path[0])
				goto skip_iterate;

			bool is_manager = is_manager_apk(candidate_path);
			pr_info("Found manager base.apk at path: %s, is_manager: %d\n", candidate_path, is_manager);

			if (unlikely(!is_manager))
				goto skip_iterate;

			crown_manager(candidate_path, uid_data);

		skip_iterate:
			list_del(&pos->list);
			if (pos != &data)
				kfree(pos);
		}
	}

	// Remove stale cached APK entries
	list_for_each_entry_safe (pos, n, &apk_path_hash_list, list) {
		if (!pos->exists) {
			list_del(&pos->list);
			kfree(pos);
		}
	}
}

static bool is_uid_exist(uid_t uid, char *package, void *data)
{
	struct list_head *list = (struct list_head *)data;
	struct uid_data *np;

	bool exist = false;
	list_for_each_entry (np, list, list) {
		if (np->uid == uid % PER_USER_RANGE &&
			strncmp(np->package, package, KSU_MAX_PACKAGE_NAME) == 0) {
			exist = true;
			break;
		}
	}
	return exist;
}

void track_throne(bool prune_only)
{
	mutex_lock(&throne_tracker_mutex);
	const struct cred *old_cred = override_creds(ksu_cred);
	struct file *fp = filp_open(SYSTEM_PACKAGES_LIST_PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: open " SYSTEM_PACKAGES_LIST_PATH " failed: %ld\n", __func__,
			PTR_ERR(fp));
		goto out_revert_cred;
	}

	struct list_head uid_list;
	INIT_LIST_HEAD(&uid_list);

	char chr = 0;
	loff_t pos = 0;
	loff_t line_start = 0;
	char buf[KSU_MAX_PACKAGE_NAME];
	for (;;) {
		ssize_t count = kernel_read(fp, &chr, sizeof(chr), &pos);
		if (count != sizeof(chr))
			break;
		if (chr != '\n')
			continue;

		count = kernel_read(fp, buf, sizeof(buf) - 1, &line_start);
		if (count <= 0) {
			break;
		}
		buf[count] = '\0';

		struct uid_data *data = kzalloc(sizeof(struct uid_data), GFP_KERNEL);
		if (!data) {
			filp_close(fp, 0);
			goto out;
		}

		char *tmp = buf;
		const char *delim = " ";
		char *package = strsep(&tmp, delim);
		char *uid = strsep(&tmp, delim);
		if (!uid || !package) {
			kfree(data);
			pr_err("update_uid: package or uid is NULL!\n");
			break;
		}

		u32 res;
		if (kstrtou32(uid, 10, &res)) {
			kfree(data);
			pr_err("update_uid: uid parse err\n");
			break;
		}
		data->uid = res;
		strscpy(data->package, package, sizeof(data->package));
		list_add_tail(&data->list, &uid_list);
		// reset line start
		line_start = pos;
	}
	filp_close(fp, 0);

	// now update uid list
	struct uid_data *np;
	struct uid_data *n;

	if (prune_only)
		goto prune;

	/*
	 * Dynamic Multi-Manager Pruning & Synchronization:
	 * Check every crowned manager in ksu_managers against uid_list.
	 * If any manager was uninstalled, remove it from ksu_managers and ksu_manager_appids.
	 */
	int i, j;
	bool need_rescan = false;
	static const char *const known_manager_pkgs[] = {
		"me.weishu.kernelsu",
		"io.github.rifsxd.ksunext",
		"com.rifsxd.ksunext",
		"org.kernelsu.next",
	};

	for (i = 0; i < ksu_crowned_count; ) {
		bool still_installed = false;
		list_for_each_entry (np, &uid_list, list) {
			if (np->uid == ksu_managers[i].uid &&
			    strncmp(np->package, ksu_managers[i].package, KSU_MAX_PACKAGE_NAME) == 0) {
				still_installed = true;
				break;
			}
		}
		if (!still_installed) {
			pr_info("Manager %s (uid=%d) uninstalled, removing!\n",
				ksu_managers[i].package, ksu_managers[i].uid);
			for (j = i; j < ksu_crowned_count - 1; j++) {
				ksu_managers[j] = ksu_managers[j + 1];
			}
			ksu_crowned_count--;
			need_rescan = true;
		} else {
			i++;
		}
	}

	/* Rebuild ksu_manager_appids and manager_pkg from remaining ksu_managers */
	ksu_manager_count = 0;
	for (i = 0; i < ksu_crowned_count && i < KSU_MAX_MANAGERS; i++) {
		ksu_manager_appids[ksu_manager_count++] = ksu_managers[i].uid;
	}
	if (ksu_crowned_count > 0) {
		ksu_manager_appid = ksu_managers[0].uid;
		strscpy(manager_pkg, ksu_managers[0].package, sizeof(manager_pkg));
	} else {
		ksu_manager_appid = (uid_t)KSU_INVALID_APPID;
		manager_pkg[0] = '\0';
		cached_manager_apk_path[0] = '\0';
		need_rescan = true;
	}

	/* Check if known manager packages are installed in packages.list but not yet crowned */
	list_for_each_entry (np, &uid_list, list) {
		for (j = 0; j < ARRAY_SIZE(known_manager_pkgs); j++) {
			if (strncmp(np->package, known_manager_pkgs[j], KSU_MAX_PACKAGE_NAME) == 0) {
				bool crowned = false;
				for (i = 0; i < ksu_crowned_count; i++) {
					if (ksu_managers[i].uid == np->uid) {
						crowned = true;
						break;
					}
				}
				if (!crowned) {
					pr_info("Known manager %s (uid=%d) found un-crowned, need scan!\n",
						np->package, np->uid);
					need_rescan = true;
				}
			}
		}
	}

	if (need_rescan) {
		pr_info("Searching manager in /data/app (crowned=%d)...\n", ksu_crowned_count);
		search_manager("/data/app", 2, &uid_list);
		pr_info("Search finished. Crowned count: %d\n", ksu_crowned_count);
	}

prune:
	// then prune the allowlist
	ksu_prune_allowlist(is_uid_exist, &uid_list);
out:
	// free uid_list
	list_for_each_entry_safe (np, n, &uid_list, list) {
		list_del(&np->list);
		kfree(np);
	}
out_revert_cred:
	revert_creds(old_cred);
	mutex_unlock(&throne_tracker_mutex);
}

void __init ksu_throne_tracker_init()
{
	struct apk_path_hash *pos, *n;

	list_for_each_entry_safe (pos, n, &apk_path_hash_list, list) {
		list_del(&pos->list);
		kfree(pos);
	}
}

void __exit ksu_throne_tracker_exit()
{
	// nothing to do
}