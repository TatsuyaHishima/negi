#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>
#include <linux/if_link.h>
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



	}
	free(a);

	rtnl_close(&rth);

	return 0;
}