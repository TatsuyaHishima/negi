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
	json_t *RTA_json = json_object();
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
		int	len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*rtm));

		json_t *route_json = json_object();
		json_t *routemsg_json = json_object();

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

		if (tb[RTA_DST]) {
			if (rtm->rtm_dst_len != host_len) {
				if (rtm->rtm_family == AF_INET) {
					unsigned char *a = RTA_DATA(tb[RTA_DST]);
					char buf[64];
					sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
					json_object_set_new(rta_json, "DST", json_string(buf));
					// printf("DST -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

				}
				else if (rtm->rtm_family == AF_INET6) {
					unsigned char *a = RTA_DATA(tb[RTA_DST]);
					char buf[64];
					inet_ntop(AF_INET6, a, buf, sizeof(buf));
					json_object_set_new(rta_json, "DST", json_string(buf));
					// printf("DST -> %s\n", buf);
				}
			// } else {
				// fprintf(stderr, "%s ", format_host(r->rtm_family,
				// 	RTA_PAYLOAD(tb[RTA_DST]),
				// 	RTA_DATA(tb[RTA_DST]),
				// 	abuf, sizeof(abuf))
				// );
			}
		}

		if(tb[RTA_SRC]) {
			if (rtm->rtm_src_len != host_len) {
				if (rtm->rtm_family == AF_INET) {
					unsigned char *a = RTA_DATA(tb[RTA_SRC]);
					char buf[64];
					sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
					json_object_set_new(rta_json, "SRC", json_string(buf));
					// printf("SRC -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

				}
				else if (rtm->rtm_family == AF_INET6) {
					unsigned char *a = RTA_DATA(tb[RTA_SRC]);
					char buf[64];
					inet_ntop(AF_INET6, a, buf, sizeof(buf));
					json_object_set_new(rta_json, "SRC", json_string(buf));
					// printf("SRC -> %s\n", buf);
				}
			}
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
			if (rtm->rtm_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[RTA_GATEWAY]);
				char buf[64];
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "GATEWAY", json_string(buf));
				// printf("GATEWAY -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);
			}
			else if (rtm->rtm_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[RTA_GATEWAY]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				json_object_set_new(rta_json, "GATEWAY", json_string(buf));
				// printf("GATEWAY -> %s\n", buf);
			}
		}

		if (tb[RTA_PRIORITY]) {
			// fprintf(stderr, "metric %d\n", *(__u32*)RTA_DATA(tb[RTA_PRIORITY]));
			json_object_set_new(rta_json, "PRIORITY", json_integer(*(__u32*)RTA_DATA(tb[RTA_PRIORITY])));
		}

		if (tb[RTA_PREFSRC]) {
			if (rtm->rtm_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[RTA_PREFSRC]);
				char buf[64];
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "PREFSRC", json_string(buf));
				// printf("PREFSRC -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (rtm->rtm_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[RTA_PREFSRC]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				json_object_set_new(rta_json, "PREFSRC", json_string(buf));
				// printf("PREFSRC -> %s\n", buf);
			}
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

	json_object_set_new(RTA_json, "RTA", route_array);

	// print json
	char *json_data = json_dumps(RTA_json, JSON_INDENT(4));
	sprintf(json_data, "%s\n", json_data); // add new line to end of file
	printf("%s", json_data);

	free(r);
	rtnl_close(&rth);
	return 0;
}
