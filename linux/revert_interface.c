#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if_arp.h>

#include "lib/iproute/libnetlink.h"
// #include <linux/netdevice.h> // for lt Linux 2.4
#include <jansson.h>

#include "common.h"
#include "revert_interface.h"
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

int delete_virtual_interface(int max_index_num) {
	int soc;
	struct sockaddr_nl sa;
	char buf[4096];
	int seq = 100;
	int i, n;

	struct nlmsghdr *nlhdr;
	struct ifinfomsg *ifihdr;

	// Make Netlink Socket
	soc = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	for (i = 1; i <= max_index_num; i++) {
		// Preparing sockaddr_nl
		memset(&sa, 0, sizeof(sa));
		sa.nl_family = AF_NETLINK; // use netlink
		sa.nl_pid = 0; // kernel
		sa.nl_groups = 0; // unicast

		memset(buf, 0, sizeof(buf));
		nlhdr = (struct nlmsghdr *)buf;
		ifihdr = NLMSG_DATA(nlhdr);

		nlhdr->nlmsg_type = RTM_DELLINK;
		nlhdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
		nlhdr->nlmsg_pid = 0;
		seq++;
		nlhdr->nlmsg_seq = seq;

		nlhdr->nlmsg_len = sizeof(struct nlmsghdr) + NLMSG_LENGTH(sizeof(struct ifinfomsg));

		ifihdr->ifi_index = i;
		ifihdr->ifi_family = AF_UNSPEC;

		// Send Netlink Message to Kernel
		n = sendto(soc, (void *)nlhdr, nlhdr->nlmsg_len, 0, (struct sockaddr *)&sa, sizeof(sa));
		if (n < 0) {
			printf("%d:\n", i);
			perror("sendto");
			return -1;
		}

		n = recv(soc, buf, sizeof(buf), 0);
		if (n < 0) {
			perror("recv");
			return -1;
		}

		for (nlhdr = (struct nlmsghdr *)buf; NLMSG_OK(nlhdr, n); nlhdr = NLMSG_NEXT(nlhdr, n)) {
			if (nlhdr->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *errmsg;
				errmsg = NLMSG_DATA(nlhdr);
				if (errmsg->error == -95) { // Operation denied: not virtual interfaces
					continue;
				}
				else if(errmsg->error == -19) { // device not exitsts
					continue;
				}
				else if(errmsg->error == 0) { // success
					fprintf(stderr, "Delete interface %2d\n", i);
				}
				else {
					printf("error code: %d, %s\n", errmsg->error, strerror(-errmsg->error));
					perror("Netlink");
					exit(2);
				}
			}
		}
	}
	printf("Deleted all virtual interfaces\n\n");
	return 0;
}

int get_max_index() {
	int max = 0;

	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	if (rtnl_wilddump_request(&rth, AF_UNSPEC, RTM_GETLINK) < 0) {
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

		if (ifimsg->ifi_index > max) {
			max = ifimsg->ifi_index;
		}
	}
	rtnl_close(&rth);

	return max;
}

int interface_real_or_virtual(json_t *interface_json) {
	int soc;
	struct sockaddr_nl sa;
	struct {
		struct nlmsghdr nh;
		struct rtgenmsg gen;
	} request;
	int seq = 100;

	struct nlmsghdr *nlhdr;

	struct sockaddr_nl kernel;
	struct msghdr rtnl_msg;
	struct iovec io;
	memset(&rtnl_msg, 0, sizeof(rtnl_msg));
	memset(&kernel, 0, sizeof(kernel));

	// Make Request Message to Kernel
	memset(&request, 0, sizeof(request));
	kernel.nl_family = AF_NETLINK; /* fill-in kernel address (destination) */

	// Make Netlink Socket
	soc = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	// Preparing sockaddr_nl
	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK; // use netlink
	sa.nl_pid = 0; // kernel
	sa.nl_groups = 0; // unicast

	request.nh.nlmsg_len = sizeof(request);
	request.nh.nlmsg_type = RTM_GETLINK;
	request.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	request.nh.nlmsg_pid = 0; // kernel
	request.nh.nlmsg_seq = seq; // need to hack
	request.gen.rtgen_family = AF_PACKET;

	io.iov_base = &request;
	io.iov_len = request.nh.nlmsg_len;
	rtnl_msg.msg_iov = &io;
	rtnl_msg.msg_iovlen = 1;
	rtnl_msg.msg_name = &kernel;
	rtnl_msg.msg_namelen = sizeof(kernel);

	// Send Netlink Message to Kernel
	sendmsg(soc, (struct msghdr *) &rtnl_msg, 0);

	// Receive Message from Kernel
	int end = 0;
	char buf[IFLIST_REPLY_BUFFER];

	while (!end) {
		int len;
		struct msghdr rtnl_reply;    /* generic msghdr structure */
		struct iovec io_reply;

		memset(&io_reply, 0, sizeof(io_reply));
		memset(&rtnl_reply, 0, sizeof(rtnl_reply));

		io.iov_base = buf;
		io.iov_len = IFLIST_REPLY_BUFFER;
		rtnl_reply.msg_iov = &io;
		rtnl_reply.msg_iovlen = 1;
		rtnl_reply.msg_name = &kernel;
		rtnl_reply.msg_namelen = sizeof(kernel);

    	len = recvmsg(soc, &rtnl_reply, 0); /* read lots of data */
		if (len < 1) {
			perror("recvmsg");
			return -1;
		}

		// Analyze Netlink Message
		for (nlhdr = (struct nlmsghdr *) buf; NLMSG_OK(nlhdr, len); nlhdr = NLMSG_NEXT(nlhdr, len)) {
			switch (nlhdr->nlmsg_type) {

				case 3:
				end = 1;
				break;

				case 16:
				end = 0;
				struct ifinfomsg *ifimsg;
				if (nlhdr->nlmsg_type != RTM_NEWLINK) {
					printf("error: %d\n", nlhdr->nlmsg_type);
					continue;
				}

				// Process Data of Netlink Message as interface
				ifimsg = NLMSG_DATA(nlhdr);
				if (ifimsg->ifi_family != AF_UNSPEC && ifimsg->ifi_family != AF_INET6) {
					printf("error family: %d\n", ifimsg->ifi_family);
					continue;
				}


				if (json_object_get(interface_json, "ifIndex") != NULL) {
					int interface_index = (int)json_number_value(json_object_get(interface_json, "ifIndex"));
					if (ifimsg->ifi_index == interface_index) {
						return 1; // real interface
					}
				}

				break;

				default:
				printf("message type %d, length %d\n", nlhdr->nlmsg_type, nlhdr->nlmsg_len);
				break;
			}
		}
		return 2; // virtual interface
	}
	close(soc);

	return -1;
}

int down_interface(int index) {

	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	struct iplink_req req;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.ifi.ifi_family = AF_UNSPEC;
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_EXCL;
	req.n.nlmsg_type = RTM_SETLINK;
	req.ifi.ifi_index = index;
	req.ifi.ifi_change |= IFF_UP;
	req.ifi.ifi_flags &= ~IFF_UP;

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
		exit(2);
	}
	fprintf(stderr, "Down interface %d\n", index);
	return 0;
}

int read_interface_file(json_t* interfaces_json) {

	// delete virtual interfaces before remake
	delete_virtual_interface(get_max_index());

	json_t *ifTable_json = json_object_get(interfaces_json, "ifTable");
	json_t *ifEntry_json = json_object_get(ifTable_json, "ifEntry");
	int i;
	for (i = 0; i < (int)json_array_size(ifEntry_json); i++) {
		json_t *interface_json = json_array_get(ifEntry_json, i);
		json_t *linux_json = json_object_get(interface_json, "linux");

		struct rtnl_handle rth = { .fd = -1 };

		if (rtnl_open(&rth, 0) < 0) {
			exit(1);
		}

		struct iplink_req req;

		memset(&req, 0, sizeof(req));
		req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
		req.ifi.ifi_family = AF_UNSPEC;
		req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
		req.ifi.ifi_index = (int)json_number_value(json_object_get(interface_json, "ifIndex"));
		req.ifi.ifi_change =  0xFFFFFFFF;
		req.ifi.ifi_type = (unsigned short)json_number_value(json_object_get(linux_json, "ifi_type"));//0;
		req.ifi.ifi_flags = (unsigned int)json_number_value(json_object_get(linux_json, "ifi_flags"));

		int real_or_virtual = interface_real_or_virtual(interface_json);

		if (real_or_virtual < 0) {
			fprintf(stderr, "Error: can't judge interface is virtual or not\n");
		}

		if (real_or_virtual == 1) { // real
			req.n.nlmsg_type = RTM_SETLINK;
		}
		else { // virtual
			req.n.nlmsg_type = RTM_NEWLINK;
		}

		char *interface_name = NULL;
		const char *key;
		json_t *value;
		char abuf[32];
		json_object_foreach(interface_json, key, value) {

			if (strcmp(key, "ifDscr") == 0) {
				interface_name = (char *)json_string_value(value);
				addattr_l(&req.n, sizeof(req), IFLA_IFNAME, (char *)json_string_value(value), strlen(json_string_value(value))+1);
			}

			if (strcmp(key, "ifMtu") == 0) {
				int mtu = (int)json_number_value(value);
				addattr_l(&req.n, sizeof(req), IFLA_MTU, &mtu, 4);
			}

			static const char *oper_states[] = { // link state あとで
				"UNKNOWN", "NOTPRESENT", "DOWN", "LOWERLAYERDOWN",
				"TESTING", "DORMANT",	 "UP"
			};

			if (strcmp(key, "ifOperStatus") == 0) {
				char *operstate = (char *)json_string_value(value);
				int i;
				for (i = 0; i < (int)sizeof(oper_states); i++) {
					if (strcmp(operstate, oper_states[i]) == 0) {
						break;
					}
				}
				switch (i) {
					case 2: // DOWN
					req.ifi.ifi_change |= IFF_UP;
					req.ifi.ifi_flags &= ~IFF_UP;
					break;
					case 6: // UP
					req.ifi.ifi_change |= IFF_UP;
					req.ifi.ifi_flags |= IFF_UP;
					break;
					default:
					break;
				}
			}

			if (strcmp(key, "ifPhysAddress") == 0) {
				if (real_or_virtual == 1) {
					continue;
				}
				int len = ll_addr_a2n(abuf, sizeof(abuf), (char *)json_string_value(value));
				if (len < 0) {
					return -1;
				}
				addattr_l(&req.n, sizeof(req), IFLA_ADDRESS, abuf, len);
			}

			if (strcmp(key, "INFO_KIND") == 0) {
				char *type = (char *)json_string_value(value);
				struct rtattr *linkinfo;
				linkinfo = addattr_nest(&req.n, sizeof(req), IFLA_LINKINFO);
				addattr_l(&req.n, sizeof(req), IFLA_INFO_KIND, type, strlen(type));
				addattr_nest_end(&req.n, linkinfo);
			}

		}
		if (real_or_virtual == 1) {
			down_interface(req.ifi.ifi_index);
		}
		fprintf(stderr, "%d: interface \"%s\" message is ready.\n", req.ifi.ifi_index, interface_name);

		if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
			exit(2);
		}

		fprintf(stderr, "%d: interface \"%s\" was changed.\n\n", req.ifi.ifi_index, interface_name);

	}
	fprintf(stderr, "Success arranging all interfaces!\n\n");

	return 0;
}
