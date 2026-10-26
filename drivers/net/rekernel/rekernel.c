// SPDX-License-Identifier: GPL-2.0
/*
 * Re:Kernel - in-tree integration for non-GKI kernels.
 *
 * Original project: https://github.com/Sakion-Team/Re-Kernel
 * Adapted for the WestCoast 4.14 kernel.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cred.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include <linux/netlink.h>
#include <net/net_namespace.h>
#include <net/netlink.h>
#include <net/sock.h>

#ifdef CONFIG_REKERNEL_NETWORK
#include <linux/in.h>
#include <linux/ipv6.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <net/inet_sock.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/rtnetlink.h>
#include <net/tcp.h>
#endif /* CONFIG_REKERNEL_NETWORK */

#include "rekernel.h"

static const char * const binder_type_names[] = {
	"reply",
	"transaction",
	"free_buffer_full",
};

static struct sock *netlink_socket;
static int netlink_unit = NETLINK_REKERNEL_MIN;

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *rekernel_dir;
static struct proc_dir_entry *rekernel_unit_entry;
static struct proc_dir_entry *rekernel_version_entry;

static int rekernel_unit_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", netlink_unit);
	return 0;
}

static int rekernel_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", REKERNEL_VERSION);
	return 0;
}

static int rekernel_unit_open(struct inode *inode, struct file *file)
{
	return single_open(file, rekernel_unit_show, NULL);
}

static int rekernel_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, rekernel_version_show, NULL);
}

static const struct file_operations rekernel_unit_fops = {
	.open		= rekernel_unit_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.owner		= THIS_MODULE,
};

static const struct file_operations rekernel_version_fops = {
	.open		= rekernel_version_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.owner		= THIS_MODULE,
};
#endif /* CONFIG_PROC_FS */

static void rekernel_remove_proc(void)
{
#ifdef CONFIG_PROC_FS
	if (rekernel_unit_entry) {
		proc_remove(rekernel_unit_entry);
		rekernel_unit_entry = NULL;
	}

	if (rekernel_version_entry) {
		proc_remove(rekernel_version_entry);
		rekernel_version_entry = NULL;
	}

	if (rekernel_dir) {
		proc_remove(rekernel_dir);
		rekernel_dir = NULL;
	}
#endif /* CONFIG_PROC_FS */
}

#ifdef CONFIG_REKERNEL_NETWORK
struct rekernel_net_uid {
	uid_t uid;
	struct list_head list;
	struct rcu_head rcu;
};

static LIST_HEAD(rekernel_net_uids);
static DEFINE_MUTEX(rekernel_net_uids_lock);

static bool rekernel_net_uid_monitored(uid_t uid)
{
	struct rekernel_net_uid *entry;
	bool found = false;

	rcu_read_lock();
	list_for_each_entry_rcu(entry, &rekernel_net_uids, list) {
		if (entry->uid == uid) {
			found = true;
			break;
		}
	}
	rcu_read_unlock();

	return found;
}

static void rekernel_net_uid_add(uid_t uid)
{
	struct rekernel_net_uid *entry, *new_entry;

	new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
	if (!new_entry)
		return;

	mutex_lock(&rekernel_net_uids_lock);
	list_for_each_entry(entry, &rekernel_net_uids, list) {
		if (entry->uid == uid) {
			mutex_unlock(&rekernel_net_uids_lock);
			kfree(new_entry);
			return;
		}
	}
	new_entry->uid = uid;
	list_add_rcu(&new_entry->list, &rekernel_net_uids);
	mutex_unlock(&rekernel_net_uids_lock);
}

static void rekernel_net_uid_del(uid_t uid)
{
	struct rekernel_net_uid *entry;

	mutex_lock(&rekernel_net_uids_lock);
	list_for_each_entry(entry, &rekernel_net_uids, list) {
		if (entry->uid == uid) {
			list_del_rcu(&entry->list);
			kfree_rcu(entry, rcu);
			break;
		}
	}
	mutex_unlock(&rekernel_net_uids_lock);
}

static void rekernel_net_uid_clear(void)
{
	struct rekernel_net_uid *entry, *tmp;

	mutex_lock(&rekernel_net_uids_lock);
	list_for_each_entry_safe(entry, tmp, &rekernel_net_uids, list) {
		list_del_rcu(&entry->list);
		kfree_rcu(entry, rcu);
	}
	mutex_unlock(&rekernel_net_uids_lock);
}
#endif /* CONFIG_REKERNEL_NETWORK */

static void netlink_rcv_msg(struct sk_buff *socket_buffer)
{
	struct nlmsghdr *nlhdr;
	struct rekernel_cmd *cmd;

	if (socket_buffer->len < nlmsg_total_size(sizeof(*cmd)))
		return;

	nlhdr = nlmsg_hdr(socket_buffer);
	cmd = nlmsg_data(nlhdr);

	switch (cmd->type) {
	case REKERNEL_CMD_REMOVE_PROC:
		/*
		 * The official legacy server ignores this command. Keep
		 * /proc/rekernel around so that userspace can always find
		 * the netlink unit, even after the manager restarts.
		 */
		break;
#ifdef CONFIG_REKERNEL_NETWORK
	case REKERNEL_CMD_ADD_MONITOR_NET:
	case REKERNEL_CMD_DEL_MONITOR_NET: {
		struct rekernel_monitor_net_args *args;

		if (socket_buffer->len <
		    nlmsg_total_size(sizeof(*cmd) + sizeof(*args)))
			break;

		args = (struct rekernel_monitor_net_args *)
			((char *)cmd + sizeof(*cmd));

		if (cmd->type == REKERNEL_CMD_DEL_MONITOR_NET)
			rekernel_net_uid_del((uid_t)args->uid);
		else
			rekernel_net_uid_add((uid_t)args->uid);
		break;
	}
#endif /* CONFIG_REKERNEL_NETWORK */
	default:
		break;
	}
}

static struct netlink_kernel_cfg rekernel_cfg = {
	.input = netlink_rcv_msg,
};

static int rekernel_send_netlink_message(const char *msg, int len)
{
	struct sk_buff *skbuffer;
	struct nlmsghdr *nlhdr;

	if (!netlink_socket || len <= 0)
		return LINE_ERROR;

#ifdef CONFIG_REKERNEL_DEBUG
	pr_info("Re:Kernel: sending %s\n", msg);
#endif

	skbuffer = nlmsg_new(len, GFP_ATOMIC);
	if (!skbuffer) {
		pr_err("Re:Kernel: netlink allocation failed\n");
		return LINE_ERROR;
	}

	nlhdr = nlmsg_put(skbuffer, 0, 0, netlink_unit, len, 0);
	if (!nlhdr) {
		nlmsg_free(skbuffer);
		return LINE_ERROR;
	}

	memcpy(nlmsg_data(nlhdr), msg, len);

	return netlink_unicast(netlink_socket, skbuffer, USER_PORT,
			       MSG_DONTWAIT);
}

static int start_rekernel_server(void)
{
	if (netlink_socket)
		return LINE_SUCCESS;

	for (netlink_unit = NETLINK_REKERNEL_MIN;
	     netlink_unit < NETLINK_REKERNEL_MAX; netlink_unit++) {
		netlink_socket = netlink_kernel_create(&init_net, netlink_unit,
						       &rekernel_cfg);
		if (netlink_socket)
			break;
	}

	if (!netlink_socket) {
		netlink_unit = NETLINK_REKERNEL_MIN;
		pr_err("Re:Kernel: failed to create the netlink server\n");
		return LINE_ERROR;
	}

	pr_info("Re:Kernel: netlink server ready (unit %d)\n", netlink_unit);

#ifdef CONFIG_PROC_FS
	rekernel_dir = proc_mkdir("rekernel", NULL);
	if (!rekernel_dir) {
		pr_err("Re:Kernel: failed to create /proc/rekernel\n");
		return LINE_SUCCESS;
	}

	{
		char unit_name[16];

		snprintf(unit_name, sizeof(unit_name), "%d", netlink_unit);
		rekernel_unit_entry = proc_create(unit_name, 0444,
						  rekernel_dir,
						  &rekernel_unit_fops);
		if (!rekernel_unit_entry)
			pr_err("Re:Kernel: failed to create /proc/rekernel/%s\n",
			       unit_name);
	}

	rekernel_version_entry = proc_create("version", 0444, rekernel_dir,
					     &rekernel_version_fops);
	if (!rekernel_version_entry)
		pr_err("Re:Kernel: failed to create /proc/rekernel/version\n");
#endif /* CONFIG_PROC_FS */

	return LINE_SUCCESS;
}

void rekernel_report(int reporttype, int type, pid_t src_pid,
		     struct task_struct *src, pid_t dst_pid,
		     struct task_struct *dst, bool oneway,
		     const char *rpc_name, __u32 code)
{
	char kmsg[PACKET_SIZE];

	if (!src || !dst)
		return;

	if (!netlink_socket)
		return;

	if (!frozen_task_group(dst))
		return;

	switch (reporttype) {
	case BINDER:
		if ((unsigned int)type >= ARRAY_SIZE(binder_type_names))
			return;
		snprintf(kmsg, sizeof(kmsg),
			 "type=Binder,bindertype=%s,oneway=%d,from_pid=%d,from=%u,target_pid=%d,target=%u,rpc_name=%s,code=%u;",
			 binder_type_names[type], oneway, src_pid,
			 (unsigned int)task_uid(src).val, dst_pid,
			 (unsigned int)task_uid(dst).val,
			 rpc_name ? rpc_name : "", code);
		break;
	case SIGNAL:
		snprintf(kmsg, sizeof(kmsg),
			 "type=Signal,signal=%d,killer_pid=%d,killer=%u,dst_pid=%d,dst=%u;",
			 type, src_pid, (unsigned int)task_uid(src).val,
			 dst_pid, (unsigned int)task_uid(dst).val);
		break;
	default:
		return;
	}

	rekernel_send_netlink_message(kmsg, strlen(kmsg));
}

#ifdef CONFIG_REKERNEL_NETWORK
static void rekernel_report_network(uid_t uid, u8 version, int data_len)
{
	char kmsg[PACKET_SIZE];

	if (!netlink_socket)
		return;

	snprintf(kmsg, sizeof(kmsg),
		 "type=Network,target=%u,proto=ipv%u,data_len=%d;",
		 (unsigned int)uid, (unsigned int)version, data_len);

	rekernel_send_netlink_message(kmsg, strlen(kmsg));
}

static unsigned int rekernel_pkg_in(void *priv, struct sk_buff *socket_buffer,
				    const struct nf_hook_state *state)
{
	struct net_device *dev = NULL;
	struct sock *sk;
	struct iphdr *iph4;
#if IS_REACHABLE(CONFIG_IPV6)
	struct ipv6hdr *iph6;
	unsigned int thoff = 0;
	unsigned short frag_off = 0;
#endif
	struct tcphdr *th;
	unsigned int ip_hdr_len;
	uid_t uid;
	u8 version;
	int data_len;

	if (!socket_buffer || !socket_buffer->len || !state)
		return NF_ACCEPT;

	if (state->hook == NF_INET_LOCAL_IN)
		dev = state->in;

	if (!dev)
		return NF_ACCEPT;

	sk = skb_to_full_sk(socket_buffer);
	if (!sk || !sk_fullsock(sk))
		return NF_ACCEPT;

	uid = sock_i_uid(sk).val;
	if (uid < MIN_USERAPP_UID)
		return NF_ACCEPT;

	if (!rekernel_net_uid_monitored(uid))
		return NF_ACCEPT;

	version = ip_hdr(socket_buffer)->version;

	if (version == 4) {
		if (!pskb_may_pull(socket_buffer, sizeof(struct iphdr)))
			return NF_ACCEPT;

		iph4 = ip_hdr(socket_buffer);
		if (iph4->protocol != IPPROTO_TCP)
			return NF_ACCEPT;

		ip_hdr_len = iph4->ihl << 2;
		if (ip_hdr_len < sizeof(struct iphdr) ||
		    !pskb_may_pull(socket_buffer,
				   ip_hdr_len + sizeof(struct tcphdr)))
			return NF_ACCEPT;

		iph4 = ip_hdr(socket_buffer);
		th = (struct tcphdr *)((unsigned char *)iph4 + ip_hdr_len);
		data_len = ntohs(iph4->tot_len) - ip_hdr_len -
			   (th->doff << 2);
#if IS_REACHABLE(CONFIG_IPV6)
	} else if (version == 6) {
		if (!pskb_may_pull(socket_buffer, sizeof(struct ipv6hdr)))
			return NF_ACCEPT;

		if (ipv6_find_hdr(socket_buffer, &thoff, -1, &frag_off,
				  NULL) != IPPROTO_TCP)
			return NF_ACCEPT;

		if (!pskb_may_pull(socket_buffer,
				   thoff + sizeof(struct tcphdr)))
			return NF_ACCEPT;

		iph6 = ipv6_hdr(socket_buffer);
		th = (struct tcphdr *)(skb_network_header(socket_buffer) +
				       thoff);
		data_len = ntohs(iph6->payload_len) -
			   (thoff - sizeof(struct ipv6hdr)) -
			   (th->doff << 2);
#endif
	} else {
		return NF_ACCEPT;
	}

	if (data_len <= 0 && !th->syn && !th->fin && !th->rst)
		return NF_ACCEPT;

	rekernel_report_network(uid, version, data_len);

	return NF_ACCEPT;
}

/* Only monitor incoming packets. */
static struct nf_hook_ops rekernel_nf_ops[] = {
	{
		.hook		= rekernel_pkg_in,
		.pf		= NFPROTO_IPV4,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP_PRI_SELINUX_LAST + 1,
	},
#if IS_REACHABLE(CONFIG_IPV6)
	{
		.hook		= rekernel_pkg_in,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP6_PRI_SELINUX_LAST + 1,
	},
#endif
};

static void unregister_netfilter(void)
{
	struct net *net;

	rtnl_lock();
	for_each_net(net)
		nf_unregister_net_hooks(net, rekernel_nf_ops,
					ARRAY_SIZE(rekernel_nf_ops));
	rtnl_unlock();
}

static int register_netfilter(void)
{
	int rc = 0;
	struct net *net;

	rtnl_lock();
	for_each_net(net) {
		rc = nf_register_net_hooks(net, rekernel_nf_ops,
					   ARRAY_SIZE(rekernel_nf_ops));
		if (rc) {
			pr_err("Re:Kernel: netfilter hook failed: %d\n", rc);
			break;
		}
	}
	rtnl_unlock();

	if (rc)
		unregister_netfilter();

	return rc;
}
#endif /* CONFIG_REKERNEL_NETWORK */

static int __init rekernel_init(void)
{
	start_rekernel_server();

#ifdef CONFIG_REKERNEL_NETWORK
	if (register_netfilter())
		pr_err("Re:Kernel: failed to register netfilter hooks\n");
#endif

	return 0;
}

static void __exit rekernel_exit(void)
{
#ifdef CONFIG_REKERNEL_NETWORK
	unregister_netfilter();
#endif

	rekernel_remove_proc();

#ifdef CONFIG_REKERNEL_NETWORK
	rekernel_net_uid_clear();
#endif

	if (netlink_socket) {
		netlink_kernel_release(netlink_socket);
		netlink_socket = NULL;
	}
}

module_init(rekernel_init);
module_exit(rekernel_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nep-Timeline & AlexLiuDev233");
MODULE_DESCRIPTION("Re:Kernel: make tombstoned users get a better experience");
