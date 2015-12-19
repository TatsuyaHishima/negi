#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "lib/iproute/libnetlink.h"
#include <jansson.h>

#include "common.h"
#include "commit_address.h"
#include "lib/utils.h"

struct iplink_req {
	struct nlmsghdr n;
	struct ifaddrmsg ifa;
	char buf[256];
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

json_t* make_address_file() {
	json_t *ipAddrTable_json = json_object();
	json_t *ipAddrEntry_array = json_array();

	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	int preferred_family = AF_PACKET;

	if (rtnl_wilddump_request(&rth, preferred_family, RTM_GETADDR) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	struct nlmsg_list *ainfo = NULL;

	if (rtnl_dump_filter(&rth, store_nlmsg, &ainfo, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	struct nlmsg_list *a, *n;

	for (a = ainfo; a; a = n) {
		n = a->next;
		struct nlmsghdr *nlhdr = &(a->h);

		struct ifaddrmsg *ifamsg = NLMSG_DATA(nlhdr);

		// Process Data of Netlink Message as ifaddrmsg
		if (ifamsg->ifa_family != AF_INET && ifamsg->ifa_family != AF_INET6) {
			printf("error family: %d\n", ifamsg->ifa_family);
			continue;
		}

		json_t *address_json = json_object();
		json_object_set_new(address_json, "ipAdEntIfIndex", json_integer(ifamsg->ifa_index));
		json_object_set_new(address_json, "ipAdEntNetMask", json_integer(ifamsg->ifa_prefixlen));

		// Analyze rtattr Message
		int len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifamsg));;

		struct rtattr *tb[IFA_MAX+1];
		parse_rtattr(tb, IFA_MAX, IFA_RTA(ifamsg), len);
		char abuf[256];

		if (tb[IFA_ADDRESS]) {
			json_object_set_new(address_json, "ipAdEntAddr", json_string(rt_addr_n2a(ifamsg->ifa_family, RTA_PAYLOAD(tb[IFA_ADDRESS]), RTA_DATA(tb[IFA_ADDRESS]), abuf, sizeof(abuf))));
		}

		if (tb[IFA_BROADCAST]) {
			json_object_set_new(address_json, "ipAdEntBcastAddr", json_string(rt_addr_n2a(ifamsg->ifa_family, RTA_PAYLOAD(tb[IFA_BROADCAST]), RTA_DATA(tb[IFA_BROADCAST]), abuf, sizeof(abuf))));
		}

		json_array_append(ipAddrEntry_array, address_json);
	}

	json_object_set_new(ipAddrTable_json, "ipAddrEntry", ipAddrEntry_array);

	free(a);
	rtnl_close(&rth);
	return ipAddrTable_json;
}
