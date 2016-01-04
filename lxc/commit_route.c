#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <jansson.h>

// #include <utils.h>

#include <stdlib.h>

// #include "lib/iproute/libnetlink.h"
#include "common.h"
#include "commit_route.h"
#include "lib/utils.h"

struct iplink_req {
	struct nlmsghdr n;
	struct rtmsg rtm;
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

int calc_host_len(struct rtmsg *r)
{
	if (r->rtm_family == AF_INET6)
		return 128;
	else if (r->rtm_family == AF_INET)
		return 32;
	else if (r->rtm_family == AF_DECnet)
		return 16;
	else if (r->rtm_family == AF_IPX)
		return 80;
	else
		return -1;
}

json_t* make_route_file() {
	json_t *ipRouteTable_json = json_object();
	json_t *ipRouteEntry_json = json_array();

	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	if (rtnl_wilddump_request(&rth, AF_PACKET, RTM_GETROUTE) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	struct nlmsg_list *rinfo = NULL;

	if (rtnl_dump_filter(&rth, store_nlmsg, &rinfo, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	struct nlmsg_list *r, *n;

	for (r = rinfo; r; r = n) {
		n = r->next;
		struct nlmsghdr *nlhdr = &(r->h);

		if (nlhdr->nlmsg_type != RTM_NEWROUTE && nlhdr->nlmsg_type != RTM_DELROUTE) {
			fprintf(stderr, "Not a route: %08x %08x %08x\n", nlhdr->nlmsg_len, nlhdr->nlmsg_type, nlhdr->nlmsg_flags);
			return 0;
		}

		struct rtmsg *rtm = NLMSG_DATA(nlhdr);
		int	len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(struct rtmsg));

		json_t *route_json = json_object();
		json_t *linux_json = json_object();

		json_object_set_new(route_json, "ipRouteType", json_integer(rtm->rtm_type));
		json_object_set_new(route_json, "ipRouteProto", json_integer(rtm->rtm_protocol));
		json_object_set_new(linux_json, "rtm_scope", json_integer(rtm->rtm_scope));
		json_object_set_new(linux_json, "rtm_table", json_integer(rtm->rtm_table));

		json_object_set_new(route_json, "linux", linux_json);

		if (len < 0) {
			fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
			exit(2);
		}

		int host_len = calc_host_len(rtm);

		// Analyze rtattr Message
		struct rtattr *tb[RTA_MAX+1];
		parse_rtattr(tb, RTA_MAX, RTM_RTA(rtm), len);

		char abuf[256];

		json_object_set_new(route_json, "ipRouteMask", json_integer(rtm->rtm_dst_len));
		if (tb[RTA_DST]) {
			if (rtm->rtm_dst_len != host_len) {
				json_object_set_new(route_json, "ipRouteDest", json_string(rt_addr_n2a(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_DST]), RTA_DATA(tb[RTA_DST]), abuf, sizeof(abuf))));
			} else {
				json_object_set_new(route_json, "ipRouteDest", json_string(format_host(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_DST]), RTA_DATA(tb[RTA_DST]), abuf, sizeof(abuf))));

			}
		} else if (rtm->rtm_dst_len) {
			json_object_set_new(route_json, "ipRouteDest", json_string("0"));
		} else {
			json_object_set_new(route_json, "ipRouteDest", json_string("default"));
		}

		if (tb[RTA_OIF]) {
			json_object_set_new(route_json, "ipRouteIfIndex", json_integer(*(int*)RTA_DATA(tb[RTA_OIF])));
		}

		if (tb[RTA_GATEWAY]) {
			json_object_set_new(route_json, "ipRouteNextHop", json_string(format_host(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_GATEWAY]), RTA_DATA(tb[RTA_GATEWAY]), abuf, sizeof(abuf))));
		}

		if (tb[RTA_PRIORITY]) {
			json_object_set_new(route_json, "ipRouteMetric1", json_integer(*(__u32*)RTA_DATA(tb[RTA_PRIORITY])));
		}

		if (tb[RTA_PREFSRC]) {
			json_object_set_new(route_json, "ipRouteInfo", json_string(rt_addr_n2a(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_PREFSRC]), RTA_DATA(tb[RTA_PREFSRC]), abuf, sizeof(abuf))));
		}
		json_array_append(ipRouteEntry_json, route_json);
	}

	json_object_set_new(ipRouteTable_json, "ipRouteEntry", ipRouteEntry_json);

	free(r);
	rtnl_close(&rth);
	return ipRouteTable_json;
}
