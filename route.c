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
#include "route.h"
#include "lib/utils.h"

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

int make_route_file(char *filename) {
	json_t *RTM_json = json_object();
	json_t *route_array = json_array();

	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	int preferred_family = AF_PACKET;

	if (rtnl_wilddump_request(&rth, preferred_family, RTM_GETROUTE) < 0) {
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

		__u32 table;

		if (nlhdr->nlmsg_type != RTM_NEWROUTE && nlhdr->nlmsg_type != RTM_DELROUTE) {
			fprintf(stderr, "Not a route: %08x %08x %08x\n", nlhdr->nlmsg_len, nlhdr->nlmsg_type, nlhdr->nlmsg_flags);
			return 0;
		}

		// printf("\n== ROUTE INFO ==\n");
		// printf("len: %d\n", nlhdr->nlmsg_len);
		// printf("type: %d\n", nlhdr->nlmsg_type);

		struct rtmsg *rtm = NLMSG_DATA(nlhdr);
		int	len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(struct rtmsg));

		json_t *route_json = json_object();
		json_t *routemsg_json = json_object();

		json_object_set_new(routemsg_json, "rtm_type", json_integer(rtm->rtm_type));
		json_object_set_new(routemsg_json, "rtm_protocol", json_integer(rtm->rtm_protocol));


		json_object_set_new(route_json, "routemsg", routemsg_json);

		if (len < 0) {
			fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
			return -1;
		}

		int host_len = calc_host_len(rtm);

		// Analyze rtattr Message
		struct rtattr *tb[RTA_MAX+1];
		parse_rtattr(tb, RTA_MAX, RTM_RTA(rtm), len);
		table = rtm_get_table(r, tb);

		json_t *rta_json = json_object();


		char abuf[256];

		json_object_set_new(rta_json, "rtm_dst_len", json_integer(rtm->rtm_dst_len));
		if (tb[RTA_DST]) {
			if (rtm->rtm_dst_len != host_len) {
				// fprintf(stdout, "%s/%u\n", rt_addr_n2a(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_DST]), RTA_DATA(tb[RTA_DST]), abuf, sizeof(abuf)), rtm->rtm_dst_len);
				json_object_set_new(rta_json, "DST", json_string(rt_addr_n2a(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_DST]), RTA_DATA(tb[RTA_DST]), abuf, sizeof(abuf))));
			} else {
				// fprintf(stdout, "%s\n", format_host(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_DST]), RTA_DATA(tb[RTA_DST]), abuf, sizeof(abuf)));
				json_object_set_new(rta_json, "DST", json_string(format_host(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_DST]), RTA_DATA(tb[RTA_DST]), abuf, sizeof(abuf))));

			}
		} else if (rtm->rtm_dst_len) {
			// fprintf(stdout, "0/%d\n", rtm->rtm_dst_len);
			json_object_set_new(rta_json, "DST", json_string("0"));
		} else {
			// fprintf(stdout, "default\n");
			json_object_set_new(rta_json, "DST", json_string("default"));
		}

		json_object_set_new(rta_json, "rtm_src_len", json_integer(rtm->rtm_src_len));
		if (tb[RTA_SRC]) {
			if (rtm->rtm_src_len != host_len) {
				fprintf(stdout, "from %s/%u ", rt_addr_n2a(rtm->rtm_family,	RTA_PAYLOAD(tb[RTA_SRC]), RTA_DATA(tb[RTA_SRC]), abuf, sizeof(abuf)), rtm->rtm_src_len);
				json_object_set_new(rta_json, "SRC", json_string(rt_addr_n2a(rtm->rtm_family,	RTA_PAYLOAD(tb[RTA_SRC]), RTA_DATA(tb[RTA_SRC]), abuf, sizeof(abuf))));
			} else {
				fprintf(stdout, "from %s ", format_host(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_SRC]), RTA_DATA(tb[RTA_SRC]), abuf, sizeof(abuf)));
				json_object_set_new(rta_json, "SRC", json_string(format_host(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_SRC]), RTA_DATA(tb[RTA_SRC]), abuf, sizeof(abuf))));
			}
		} else if (rtm->rtm_src_len) {
			fprintf(stdout, "from 0/%u ", rtm->rtm_src_len);
			json_object_set_new(rta_json, "SRC", json_string("0"));
		}

		if (tb[RTA_IIF]) {
			// printf("IIF -> %d\n", *(int*)RTA_DATA(tb[RTA_IIF]));
			json_object_set_new(rta_json, "IIF", json_integer(*(int*)RTA_DATA(tb[RTA_IIF])));
		}

		if (tb[RTA_OIF]) {
			// printf("OIF -> %d\n", *(int*)RTA_DATA(tb[RTA_OIF]));
			json_object_set_new(rta_json, "IIF", json_integer(*(int*)RTA_DATA(tb[RTA_OIF])));
		}

		if (tb[RTA_GATEWAY]) {
			// fprintf(stdout, "via %s ", format_host(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_GATEWAY]), RTA_DATA(tb[RTA_GATEWAY]), abuf, sizeof(abuf)));
			json_object_set_new(rta_json, "GATEWAY", json_string(format_host(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_GATEWAY]), RTA_DATA(tb[RTA_GATEWAY]), abuf, sizeof(abuf))));
		}

		if (tb[RTA_PRIORITY]) {
			// fprintf(stderr, "metric %d\n", *(__u32*)RTA_DATA(tb[RTA_PRIORITY]));
			json_object_set_new(rta_json, "PRIORITY", json_integer(*(__u32*)RTA_DATA(tb[RTA_PRIORITY])));
		}

		if (tb[RTA_PREFSRC]) {
			// fprintf(stdout, " src %s ",	rt_addr_n2a(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_PREFSRC]), RTA_DATA(tb[RTA_PREFSRC]), abuf, sizeof(abuf)));
			json_object_set_new(rta_json, "PREFSRC", json_string(rt_addr_n2a(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_PREFSRC]), RTA_DATA(tb[RTA_PREFSRC]), abuf, sizeof(abuf))));

			// if (rtm->rtm_family == AF_INET) {
			// 	unsigned char *a = RTA_DATA(tb[RTA_PREFSRC]);
			// 	char buf[64];
			// 	sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
			// 	json_object_set_new(rta_json, "PREFSRC", json_string(buf));
			// 	// printf("PREFSRC -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			// }
			// else if (rtm->rtm_family == AF_INET6) {
			// 	unsigned char *a = RTA_DATA(tb[RTA_PREFSRC]);
			// 	char buf[64];
			// 	inet_ntop(AF_INET6, a, buf, sizeof(buf));
			// 	json_object_set_new(rta_json, "PREFSRC", json_string(buf));
			// 	// printf("PREFSRC -> %s\n", buf);
			// }
		}

		if (tb[RTA_METRICS]) {
			printf("METRICS -> %d\n", *(int*)RTA_DATA(tb[RTA_METRICS]));
			json_object_set_new(rta_json, "METRICS", json_integer(*(int*)RTA_DATA(tb[RTA_METRICS])));
		}

		if (tb[RTA_MULTIPATH]) {

		}

		json_object_set_new(route_json, "rta", rta_json);

		json_array_append(route_array, route_json);
	}

	json_object_set_new(RTM_json, "RTM", route_array);

	// print json
	char *json_data = json_dumps(RTM_json, JSON_INDENT(4));
	sprintf(json_data, "%s\n", json_data); // add new line to end of file
	// printf("%s", json_data);

	make_file(filename, json_data);

	free(r);
	rtnl_close(&rth);
	return 0;
}

int delete_route(char *address, int netmask) {

	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	struct iplink_req {
		struct nlmsghdr     n;
		struct rtmsg    rtm;
		char            buf[1024];
	};

	struct iplink_req req;
	int preferred_family = AF_UNSPEC;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	req.n.nlmsg_type = RTM_DELROUTE;
	req.rtm.rtm_family = preferred_family;
	req.rtm.rtm_table = RT_TABLE_MAIN;
	req.rtm.rtm_scope = RT_SCOPE_NOWHERE;

	inet_prefix dst;

	get_prefix(&dst, address, req.rtm.rtm_family);
	req.rtm.rtm_family = dst.family;

	if (req.rtm.rtm_family == AF_UNSPEC) {
		req.rtm.rtm_family = AF_INET;
	}

	req.rtm.rtm_dst_len = netmask;
	if (dst.bytelen) {
		addattr_l(&req.n, sizeof(req), RTA_DST, &dst.data, dst.bytelen);
	}

	ll_init_map(&rth);

	struct nlmsghdr *answer;
	int errnum = rtnl_talkE(&rth, &req.n, 0, 0, &answer, NULL, NULL);
	if (errnum < 0) {
		exit(2);
	}

	if (answer) {
		struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(answer);
		printf("errno: %d  ", errnum);
		switch (errnum) {
			case 0: // Success
			fprintf(stderr, "delete route to %s\n", address);
			break;

			case 3: // No such device
			fprintf(stderr, "already deleted.\n");
			break;

			default:
			fprintf(stderr, "ERROR!\terrno: %d\n", errnum);
			break;
		}
	} else {
		fprintf(stderr, "Something Wrong!\n");
		exit(2);
	}

	return 0;
}

int delete_all_route() {
	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	int preferred_family = AF_PACKET;

	if (rtnl_wilddump_request(&rth, preferred_family, RTM_GETROUTE) < 0) {
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

		__u32 table;

		if (nlhdr->nlmsg_type != RTM_NEWROUTE && nlhdr->nlmsg_type != RTM_DELROUTE) {
			fprintf(stderr, "Not a route: %08x %08x %08x\n", nlhdr->nlmsg_len, nlhdr->nlmsg_type, nlhdr->nlmsg_flags);
			return 0;
		}

		struct rtmsg *rtm = NLMSG_DATA(nlhdr);
		int	len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*rtm));

		if (len < 0) {
			fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
			return -1;
		}

		int host_len = calc_host_len(rtm);

		// Analyze rtattr Message
		struct rtattr *tb[RTA_MAX+1];
		parse_rtattr(tb, RTA_MAX, RTM_RTA(rtm), len);
		table = rtm_get_table(r, tb);

		char dst_address[64] = "";
		char abuf[256];

		if (tb[RTA_DST]) {
			// if (rtm->rtm_dst_len != host_len) {
			sprintf(dst_address, "%s/%d", rt_addr_n2a(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_DST]), RTA_DATA(tb[RTA_DST]), abuf, sizeof(abuf)), rtm->rtm_dst_len);
			// } else {
				// sprintf(dst_address, "%s", format_host(rtm->rtm_family,	RTA_PAYLOAD(tb[RTA_DST]), RTA_DATA(tb[RTA_DST]), abuf, sizeof(abuf)));

			// }
		} else if (rtm->rtm_dst_len) {
			sprintf(dst_address, "0/%d", rtm->rtm_dst_len);
		} else {
			sprintf(dst_address, "default");
		}
		// printf("%s:%d\n", dst_address, rtm->rtm_dst_len);
		if (strncmp(dst_address, "127.0.0.0", 9) != 0 && strcmp(dst_address, "ff00::") != 0) {
			delete_route((char *)dst_address, rtm->rtm_dst_len);
		}
	}

	free(r);
	rtnl_close(&rth);
	return 0;
}

int read_route_file(char *filename) {
	// routeの削除
	delete_all_route();




	return 0;
}
