#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "lib/iproute/libnetlink.h"
#include <jansson.h>


#include "common.h"
#include "address.h"

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

int make_address_file(char *filename) {
	json_t *IFA_json = json_object();
	json_t *address_array = json_array();

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

		// Print Netlink Message length and type
		// printf("\n===\n");
		// printf("len: %d\n", nlhdr->nlmsg_len);
		// printf("type: %d\n", nlhdr->nlmsg_type);

		struct ifaddrmsg *ifamsg = NLMSG_DATA(nlhdr);

		// Process Data of Netlink Message as ifinfomsg
		if (ifamsg->ifa_family != AF_INET && ifamsg->ifa_family != AF_INET6) {
			printf("error family: %d\n", ifamsg->ifa_family);
			continue;
		}

		json_t *address_json = json_object();
		json_t *ifaddrmsg_json = json_object();
		json_object_set_new(ifaddrmsg_json, "interface", json_integer(ifamsg->ifa_index));

		// Add Each Parameter to json file
		// printf("index: %d\n", ifamsg->ifa_index);

		// Analyze rtattr Message
		int len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifamsg));;

		struct rtattr *tb[IFA_MAX+1];
		parse_rtattr(tb, IFA_MAX, IFA_RTA(ifamsg), len);

		json_t *rta_json = json_object();

		if (tb[IFA_LABEL]) {
			// printf("%s\n", (char *)RTA_DATA(tb[IFA_LABEL]));
			json_object_set_new(rta_json, "LABEL", json_string((char *)RTA_DATA(tb[IFA_LABEL])));
		}
		else {
			char name[8];
			if_indextoname(ifamsg->ifa_index, name);
			json_object_set_new(rta_json, "LABEL", json_string(name));
			// printf("%s\n", name);
		}

		if (tb[IFA_ADDRESS]) {
			if (ifamsg->ifa_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[IFA_ADDRESS]);
				char buf[64];
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "ADDRESS", json_string(buf));
				// printf("ADDRESS -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[IFA_ADDRESS]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				json_object_set_new(rta_json, "ADDRESS", json_string(buf));
				// printf("ADDRESS -> %s\n", buf);
			}
		}
		if (tb[IFA_LOCAL]) {
			if (ifamsg->ifa_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[IFA_LOCAL]);
				char buf[64];
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "LOCAL", json_string(buf));
				// printf("LOCAL -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[IFA_LOCAL]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				json_object_set_new(rta_json, "LOCAL", json_string(buf));
				// printf("LOCAL -> %s\n", buf);
			}
		}
		if (tb[IFA_BROADCAST]) {
			if (ifamsg->ifa_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[IFA_BROADCAST]);
				char buf[64];
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "BROADCAST", json_string(buf));
				// printf("BROADCAST ->%d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[IFA_BROADCAST]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				json_object_set_new(rta_json, "BROADCAST", json_string(buf));
				// printf("BROADCAST -> %s\n", buf);
			}
		}
		if (tb[IFA_ANYCAST]) {
			if (ifamsg->ifa_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[IFA_ANYCAST]);
				char buf[64];
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "ANYCAST", json_string(buf));				
				// printf("ANYCAST ->%d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[IFA_ANYCAST]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				json_object_set_new(rta_json, "ANYCAST", json_string(buf));			
				// printf("ANYCAST -> %s\n", buf);
			}
		}
		if (tb[IFA_CACHEINFO]) {
			struct ifa_cacheinfo *ci = RTA_DATA(tb[IFA_CACHEINFO]);
			char buf1[64];
			char buf2[64];
			// fprintf(stderr, "valid_lft ");
			if (ci->ifa_valid == INFINITY_LIFE_TIME)
				sprintf(buf1, "%s", "forever");
			else
				sprintf(buf1, "%usec", ci->ifa_valid);
			// printf("%s\n", buf1);
			json_object_set_new(rta_json, "CACHEINFO_VALID", json_string(buf1));

			// fprintf(stderr, "preferred_lft ");
			if (ci->ifa_prefered == INFINITY_LIFE_TIME)
				sprintf(buf2, "%s", "forever");
			else {
				if (ifamsg->ifa_flags&IFA_F_DEPRECATED)
					sprintf(buf2, "%dsec", ci->ifa_prefered);
				else
					sprintf(buf2, "%usec", ci->ifa_prefered);
			}
			// printf("%s\n", buf2);
			json_object_set_new(rta_json, "CACHEINFO_PREFERRED", json_string(buf2));

		}
		if (tb[IFA_MULTICAST]) {
			if (ifamsg->ifa_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[IFA_MULTICAST]);
				char buf[64];
				sprintf(buf, "%d:%d:%d:%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "MULTICAST", json_string(buf));
				// printf("MULTICAST ->%d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[IFA_MULTICAST]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				json_object_set_new(rta_json, "MULTICAST", json_string(buf));
				// printf("MULTICAST -> %s\n", buf);
			}

		}
		if (tb[IFA_FLAGS]) {
			json_object_set_new(rta_json, "FLAGS", json_integer(*(int *)RTA_DATA(tb[IFA_FLAGS])));
			// printf("FLAGS -> %d\n", *(int *)RTA_DATA(tb[IFA_FLAGS]));
		}

		json_object_set_new(address_json, "rta", rta_json);

		json_array_append(address_array, address_json);
	}

	json_object_set_new(IFA_json, "IFA", address_array);

	// print json
	char *json_data = json_dumps(IFA_json, JSON_INDENT(4));
	sprintf(json_data, "%s\n", json_data); // add new line to end of file
	printf("%s", json_data);

	free(a);

	rtnl_close(&rth);

	return 0;
}

