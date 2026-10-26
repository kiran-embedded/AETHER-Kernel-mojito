#ifndef __RE_KERNEL_INTERNAL_H
#define __RE_KERNEL_INTERNAL_H

#include <net/rekernel.h>

#define REKERNEL_VERSION	"11.7"

#define NETLINK_REKERNEL_MIN	22
#define NETLINK_REKERNEL_MAX	26
#define USER_PORT		100
#define PACKET_SIZE		256

extern struct net init_net;

enum rekernel_cmd_type {
	REKERNEL_CMD_REMOVE_PROC = 1,
	REKERNEL_CMD_ADD_MONITOR_NET = 2,
	REKERNEL_CMD_DEL_MONITOR_NET = 3,
};

struct rekernel_cmd {
	int type;
};

struct rekernel_monitor_net_args {
	int uid;
};

#endif /* __RE_KERNEL_INTERNAL_H */
