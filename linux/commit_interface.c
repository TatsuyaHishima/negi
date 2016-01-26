#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if_arp.h>

#include "lib/iproute/libnetlink.h"
// #include <linux/netdevice.h> // for lt Linux 2.4
#include <jansson.h>

#include "common.h"
#include "commit_interface.h"
#include "lib/rt_names.h"

#define IFLIST_REPLY_BUFFER 8096

struct iplink_req {
	struct nlmsghdr n;
	struct ifinfomsg ifi;
	char buf[1024];
};

static int store_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct nlmsg_list **linfo = (struct nlmsg_list**)arg;
	struct nlmsg_list *h;
	struct nlmsg_list **lp;

	h = malloc(n->nlmsg_len+sizeof(void*));
	if (h == NULL)
		return -1;

	memcpy(&h->h, n, n->nlmsg_len);
	h->next = NULL;

	for (lp = linfo; *lp; lp = &(*lp)->next) /* NOTHING */;
	*lp = h;

	return 0;
}

json_t* make_interface_file() {
	json_t *interfaces_json = json_object();
	json_t *ifTable_json = json_object();
	json_t *ifEntry_array = json_array();

	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	if (rtnl_wilddump_request(&rth, AF_PACKET, RTM_GETLINK) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	struct nlmsg_list *linfo = NULL;

	if (rtnl_dump_filter(&rth, store_nlmsg, &linfo, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	struct nlmsg_list *l, *n;

	for (l = linfo; l; l = n) {
		n = l->next;
		struct nlmsghdr *nlhdr = &(l->h);
		struct ifinfomsg *ifimsg = NLMSG_DATA(nlhdr);

		// Process Data of Netlink Message as ifinfomsg
		if (ifimsg->ifi_family != AF_UNSPEC && ifimsg->ifi_family != AF_INET6) {
			printf("error family: %d\n", ifimsg->ifi_family);
			continue;
		}

		// Add Each Parameter to json file
		json_t *interface_json = json_object();
		json_t *linux_json = json_object();

		json_object_set_new(interface_json, "ifIndex", json_integer(ifimsg->ifi_index));
		json_object_set_new(linux_json, "ifi_type", json_integer(ifimsg->ifi_type));
		json_object_set_new(linux_json, "ifi_flags", json_integer(ifimsg->ifi_flags));

		// Analyze rtattr Message
		int len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifimsg));;

		struct rtattr *tb[IFLA_MAX+1];
		parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifimsg), len);


		if (tb[IFLA_IFNAME]) {
			// printf("\n%s\n", (char *)RTA_DATA(tb[IFLA_IFNAME]));
			json_object_set_new(interface_json, "ifDscr", json_string((char *)RTA_DATA(tb[IFLA_IFNAME])));
		}

		if (tb[IFLA_MTU]) {
			// printf("%d\n", *(int *)RTA_DATA(tb[IFLA_MTU]));
			json_object_set_new(interface_json, "ifMtu", json_integer(*(int *)RTA_DATA(tb[IFLA_MTU])));
		}

		static const char *oper_states[] = {
			"UNKNOWN", "NOTPRESENT", "DOWN", "LOWERLAYERDOWN",
			"TESTING", "DORMANT",	 "UP"
		};

		if (tb[IFLA_OPERSTATE]) {
			__u8 state = *(__u8 *)RTA_DATA(tb[IFLA_OPERSTATE]);
			if (state >= sizeof(oper_states)/sizeof(oper_states[0])){
				fprintf(stderr, "state %#x ", state);
			}
			else{
				// printf("state %s ", oper_states[state]);
				json_object_set_new(interface_json, "ifOperStatus", json_string(oper_states[state]));
			}
		}

		if (tb[IFLA_ADDRESS]) {
			char abuf[64];
			json_object_set_new(interface_json, "ifPhysAddress", json_string(ll_addr_n2a(RTA_DATA(tb[IFLA_ADDRESS]), RTA_PAYLOAD(tb[IFLA_ADDRESS]), ifimsg->ifi_type, abuf, sizeof(abuf))));
		}

		if (tb[IFLA_LINKINFO]) {
			struct rtattr *linkinfo[IFLA_INFO_MAX+1];
			char *kind;

			parse_rtattr_nested(linkinfo, IFLA_INFO_MAX, tb[IFLA_LINKINFO]);

			if (!linkinfo[IFLA_INFO_KIND]){
				exit(2);
			}
			kind = RTA_DATA(linkinfo[IFLA_INFO_KIND]);
			json_object_set_new(interface_json, "INFO_KIND", json_string(kind));
		}

		json_object_set_new(interface_json, "linux", linux_json);
		json_array_append(ifEntry_array, interface_json);
	}

	json_object_set_new(ifTable_json, "ifEntry", ifEntry_array);
	json_object_set_new(interfaces_json, "ifTable", ifTable_json);

	free(l);

	rtnl_close(&rth);

	return interfaces_json;
}
