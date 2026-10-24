#ifndef _SUSFS_PRCTL_H_
#define _SUSFS_PRCTL_H_

#ifdef CONFIG_KSU_SUSFS
void susfs_on_post_fs_data(void);
int ksu_handle_prctl(int option, unsigned long arg2, unsigned long arg3,
		     unsigned long arg4, unsigned long arg5);
#endif

#endif
