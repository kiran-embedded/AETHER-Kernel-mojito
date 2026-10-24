// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
extern struct static_key_false susfs_is_fake_cmdline_or_bootconfig_buffer_set;
extern void susfs_spoof_cmdline_or_bootconfig(struct seq_file *m);
#endif

static void print_sanitized_cmdline(struct seq_file *m, const char *s)
{
	const char *pos = s;
	const char *next;
	size_t len;

	if (unlikely(!s)) {
		seq_putc(m, '\n');
		return;
	}

	while (*pos) {
		while (*pos == ' ' || *pos == '\t') {
			seq_putc(m, *pos);
			pos++;
		}
		if (!*pos || *pos == '\r' || *pos == '\n')
			break;

		next = strpbrk(pos, " \t\r\n");
		if (!next)
			next = pos + strlen(pos);
		len = next - pos;

		/* Hardware-level bootloader lock sanitization with exact token length matching */
		if ((len == sizeof("androidboot.verifiedbootstate=orange") - 1 &&
		     strncmp(pos, "androidboot.verifiedbootstate=orange", len) == 0) ||
		    (len == sizeof("androidboot.verifiedbootstate=yellow") - 1 &&
		     strncmp(pos, "androidboot.verifiedbootstate=yellow", len) == 0)) {
			seq_puts(m, "androidboot.verifiedbootstate=green");
		} else if (len == sizeof("androidboot.flash.locked=0") - 1 &&
			   strncmp(pos, "androidboot.flash.locked=0", len) == 0) {
			seq_puts(m, "androidboot.flash.locked=1");
		} else if (len == sizeof("androidboot.vbmeta.device_state=unlocked") - 1 &&
			   strncmp(pos, "androidboot.vbmeta.device_state=unlocked", len) == 0) {
			seq_puts(m, "androidboot.vbmeta.device_state=locked");
		} else if (len == sizeof("androidboot.selinux=permissive") - 1 &&
			   strncmp(pos, "androidboot.selinux=permissive", len) == 0) {
			seq_puts(m, "androidboot.selinux=enforcing");
		} else {
			seq_write(m, pos, len);
		}

		pos = next;
	}
	seq_putc(m, '\n');
}

static int cmdline_proc_show(struct seq_file *m, void *v)
{
	/*
	 * Hardware-level bootloader lock sanitization and spoofing must NOT be exposed
	 * to Android init (PID 1), system services (UID < 10000), or early boot stage.
	 * If Android libfs_avb reads vbmeta.device_state=locked on custom ROMs,
	 * it strictly enforces OEM cryptographic signatures, fails verification,
	 * and halts or panics the boot process.
	 */
	if (system_state < SYSTEM_RUNNING || current->pid <= 1 || current->tgid <= 1 || current_uid().val < 10000) {
		seq_printf(m, "%s\n", saved_command_line);
		return 0;
	}

#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	if (static_branch_likely(&susfs_is_fake_cmdline_or_bootconfig_buffer_set)) {
		susfs_spoof_cmdline_or_bootconfig(m);
		seq_putc(m, '\n');
		return 0;
	}
#endif

	print_sanitized_cmdline(m, saved_command_line);
	return 0;
}

static int cmdline_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdline_proc_show, NULL);
}

static const struct file_operations cmdline_proc_fops = {
	.open		= cmdline_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_cmdline_init(void)
{
	proc_create("cmdline", 0, NULL, &cmdline_proc_fops);
	return 0;
}
fs_initcall(proc_cmdline_init);
