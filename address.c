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
		printf("\n===\n");
		printf("len: %d\n", nlhdr->nlmsg_len);
		printf("type: %d\n", nlhdr->nlmsg_type);

		struct ifaddrmsg *ifamsg = NLMSG_DATA(nlhdr);

		// Process Data of Netlink Message as ifinfomsg
		if (ifamsg->ifa_family != AF_INET && ifamsg->ifa_family != AF_INET6) {
			printf("error family: %d\n", ifamsg->ifa_family);
			continue;
		}

		// Add Each Parameter to json file
		printf("index: %d\n", ifamsg->ifa_index);

		// Analyze rtattr Message
		int len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifamsg));;

		struct rtattr *tb[IFA_MAX+1];
		parse_rtattr(tb, IFA_MAX, IFA_RTA(ifamsg), len);


		if (tb[IFA_LABEL]) {
			printf("%s\n", (char *)RTA_DATA(tb[IFA_LABEL]));
		}
		else {
			char name[8];
			if_indextoname(ifamsg->ifa_index, name);
			printf("%s\n", name);
		}

		if (tb[IFA_ADDRESS]) {
			if (ifamsg->ifa_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[IFA_ADDRESS]);
				char buf[64];
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				printf("ADDRESS -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[IFA_ADDRESS]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				printf("ADDRESS -> %s\n", buf);
			}
		}
		if (tb[IFA_LOCAL]) {
			if (ifamsg->ifa_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[IFA_LOCAL]);
				char buf[64];
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				printf("LOCAL -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[IFA_LOCAL]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				printf("LOCAL -> %s\n", buf);
			}
		}
		if (tb[IFA_BROADCAST]) {
			if (ifamsg->ifa_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[IFA_BROADCAST]);
				char buf[64];
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				printf("BROADCAST ->%d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[IFA_BROADCAST]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				printf("BROADCAST -> %s\n", buf);
			}
		}
		if (tb[IFA_ANYCAST]) {
			if (ifamsg->ifa_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[IFA_ANYCAST]);
				char buf[64];
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				printf("ANYCAST ->%d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[IFA_ANYCAST]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				printf("ANYCAST -> %s\n", buf);
			}
		}
		if (tb[IFA_CACHEINFO]) {
			struct ifa_cacheinfo *ci = RTA_DATA(tb[IFA_CACHEINFO]);
			fprintf(stderr, "valid_lft ");
			if (ci->ifa_valid == INFINITY_LIFE_TIME)
				fprintf(stderr, "forever");
			else
				fprintf(stderr, "%usec", ci->ifa_valid);
			fprintf(stderr, " preferred_lft ");
			if (ci->ifa_prefered == INFINITY_LIFE_TIME)
				fprintf(stderr, "forever\n");
			else {
				if (ifamsg->ifa_flags&IFA_F_DEPRECATED)
					fprintf(stderr, "%dsec\n", ci->ifa_prefered);
				else
					fprintf(stderr, "%usec\n", ci->ifa_prefered);
			}

		}
		if (tb[IFA_MULTICAST]) {
			if (ifamsg->ifa_family == AF_INET) {
				unsigned char *a = RTA_DATA(tb[IFA_MULTICAST]);
				char buf[64];
				sprintf(buf, "%d:%d:%d:%d", a[0], a[1], a[2], a[3]);
				printf("MULTICAST ->%d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				unsigned char *a = RTA_DATA(tb[IFA_MULTICAST]);
				char buf[64];
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				printf("MULTICAST -> %s\n", buf);
			}

		}
		if (tb[IFA_FLAGS]) {
			printf("FLAGS -> %d\n", *(int *)RTA_DATA(tb[IFA_FLAGS]));
		}
	}
	free(a);

	rtnl_close(&rth);

	return 0;
}

