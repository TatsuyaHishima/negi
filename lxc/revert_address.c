#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "lib/iproute/libnetlink.h"
#include <jansson.h>

#include "common.h"
#include "revert_address.h"
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

int link_local(char *address) {
	if (strncmp(address, "fe80:", 5) == 0) {
		return 1;
	}
	return 0;
}

int delete_address(int interface, char *address) {

	if (link_local(address)) {
		fprintf(stderr, "Can't delete ipv6 link local address.\n");
		return 0;
	}

	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	struct iplink_req req;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.ifa.ifa_index = interface;
	req.ifa.ifa_flags = 0;
	req.ifa.ifa_scope = 0;
	req.n.nlmsg_type = RTM_DELADDR;

	inet_prefix lcl;
	get_prefix(&lcl, address, req.ifa.ifa_family);
	addattr_l(&req.n, sizeof(req), IFA_LOCAL, &lcl.data, lcl.bytelen);
	req.ifa.ifa_prefixlen = lcl.bitlen;
	req.ifa.ifa_family = lcl.family;

	ll_init_map(&rth);

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
		exit(2);
	}

	fprintf(stderr, "delete address %s\n", address);

	return 0;
}

int delete_all_address() {
	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	if (rtnl_wilddump_request(&rth, AF_PACKET, RTM_GETADDR) < 0) {
		perror("Cannot send dump request\n");
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
			fprintf(stderr, "error family: %d\n", ifamsg->ifa_family);
			continue;
		}

		// Analyze rtattr Message
		int len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifamsg));;

		struct rtattr *tb[IFA_MAX+1];
		parse_rtattr(tb, IFA_MAX, IFA_RTA(ifamsg), len);
		char buf[64] = "";
		char abuf[256];
		if (tb[IFA_ADDRESS]) {
			sprintf(buf, "%s", rt_addr_n2a(ifamsg->ifa_family, RTA_PAYLOAD(tb[IFA_ADDRESS]), RTA_DATA(tb[IFA_ADDRESS]), abuf, sizeof(abuf)));
		}
		delete_address(ifamsg->ifa_index, buf);
	}
	fprintf(stderr, "delete all addresses.(except link local addresses)\n\n");
	free(a);
	rtnl_close(&rth);
	return 0;
}

void modify_address(json_t *ipAddrEntry_json) {
	int i;
	for (i = 0; i < (int)json_array_size(ipAddrEntry_json); i++) {
		json_t *address_json = json_array_get(ipAddrEntry_json, i);

		struct rtnl_handle rth = { .fd = -1 };

		if (rtnl_open(&rth, 0) < 0) {
			exit(1);
		}

		struct iplink_req req;

		memset(&req, 0, sizeof(req));
		req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
		req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
		req.ifa.ifa_index = (int)json_number_value(json_object_get(address_json, "ipAdEntIfIndex"));
		req.ifa.ifa_prefixlen = (int)json_number_value(json_object_get(address_json, "ipAdEntNetMask"));

		req.ifa.ifa_flags = 0;
		req.ifa.ifa_family = 0;
		req.ifa.ifa_scope = 0;
		req.n.nlmsg_type = RTM_NEWADDR;

		inet_prefix addr;
		int link_local_flag = 0;
		char *address_name = "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff";

		const char *key;
		json_t *value;
		json_object_foreach(address_json, key, value) {
			if (strcmp(key, "ipAdEntAddr") == 0) {
				// if link_local -> skip
				if (link_local((char *)json_string_value(value))) {
					link_local_flag = 1;
					break;
				}

				address_name = (char *)json_string_value(value);
				get_prefix(&addr, address_name, req.ifa.ifa_family);
				addattr_l(&req.n, sizeof(req), IFA_LOCAL, &addr.data, addr.bytelen);
				addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &addr.data, addr.bytelen);

				req.ifa.ifa_family = addr.family;
			}
			if (strcmp(key, "ipAdEntBcastAddr") == 0) {
				get_addr(&addr, (char *)json_string_value(value), req.ifa.ifa_family);
				if (req.ifa.ifa_family == AF_UNSPEC)
					req.ifa.ifa_family = addr.family;
				addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &addr.data, addr.bytelen);
			}
		}

		if (link_local_flag) {
			fprintf(stderr, "Don't assign link local address.\n");
			continue;
		}

		if (req.ifa.ifa_family == 0) {
			printf("No address data\n");
			exit(5);
		}

		ll_init_map(&rth);

		if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
			exit(2);
		}
		fprintf(stderr, "arrange address %s/%d\n", address_name, req.ifa.ifa_prefixlen);
	}
	fprintf(stderr, "Success arranging all addresses!\n\n");
}

int read_address_file(json_t* addresses_json) {

	// 全部のアドレス情報を削除する必要あり
	delete_all_address();

	json_t *ipAddrEntry_json = json_object_get(addresses_json, "ipAddrEntry");
	modify_address(ipAddrEntry_json);

	return 0;
}
