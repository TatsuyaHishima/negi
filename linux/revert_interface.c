#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if_arp.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#define BRCTL_ADD_IF 4

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

int check_virtual_interface(int ifindex) {
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

		if (ifimsg->ifi_index != ifindex) {
			continue;
		}

		// Analyze rtattr Message
		int len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifimsg));;
		struct rtattr *tb[IFLA_MAX+1];
		parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifimsg), len);

		if (tb[IFLA_LINKINFO]) {
			struct rtattr *linkinfo[IFLA_INFO_MAX+1];
			char *kind;

			parse_rtattr_nested(linkinfo, IFLA_INFO_MAX, tb[IFLA_LINKINFO]);

			if (!linkinfo[IFLA_INFO_KIND]){
				return -1;
			} else {
				kind = RTA_DATA(linkinfo[IFLA_INFO_KIND]);
				if (strcmp(kind, "bridge") == 0) {
					return -1;
				} else {
					return 0;
				}
			}
		}
		return 0;
	}

	free(l);
	rtnl_close(&rth);
	return -1;
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

		if (check_virtual_interface(i) == 0) {
			continue;
		}
		ifihdr->ifi_index = i;
		ifihdr->ifi_family = AF_UNSPEC;

		// Send Netlink Message to Kernel
		n = sendto(soc, (void *)nlhdr, nlhdr->nlmsg_len, 0, (struct sockaddr *)&sa, sizeof(sa));
		if (n < 0) {
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

	if (json_object_get(interface_json, "ifIndex") == NULL) {
		fprintf(stderr, "can not get interface index from json.\n");
		return -1;
	}
	int interface_index = (int)json_number_value(json_object_get(interface_json, "ifIndex"));

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

		if (ifimsg->ifi_index == interface_index) {
			return 1; // real
		}
	}
	rtnl_close(&rth);

	return 2; // virtual
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

int br_socket_fd = -1;

int br_init(void)
{
        if ((br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0)
                return errno;
        return 0;
}

int br_add_interface(const char *bridge, const char *dev)
{
        struct ifreq ifr;
        int err;
        int ifindex = if_nametoindex(dev);

        if (ifindex == 0)
                return ENODEV;

        strncpy(ifr.ifr_name, bridge, IFNAMSIZ);
#ifdef SIOCBRADDIF
        ifr.ifr_ifindex = ifindex;
        err = ioctl(br_socket_fd, SIOCBRADDIF, &ifr);
        if (err < 0)
#endif
        {
                unsigned long args[4] = { BRCTL_ADD_IF, ifindex, 0, 0 };

                ifr.ifr_data = (char *) args;
                err = ioctl(br_socket_fd, SIOCDEVPRIVATE, &ifr);
        }

        return err < 0 ? errno : 0;
}

void br_shutdown(void)
{
        close(br_socket_fd);
        br_socket_fd = -1;
}

int br_addif(char *bridge_name, char *interface_name) {
        if (br_init()) {
                fprintf(stderr, "can't setup bridge control: %s\n",
                strerror(errno));
                return 1;
        }

        int e = br_add_interface(interface_name, bridge_name);
				switch(e) {
				case 0:
					break;

				case ENODEV:
					if (if_nametoindex(interface_name) == 0)
						fprintf(stderr, "interface %s does not exist!\n", interface_name);
					else
						fprintf(stderr, "bridge %s does not exist!\n", bridge_name);
					break;

				case EBUSY:
					fprintf(stderr,	"device %s is already a member of a bridge; "
						"can't enslave it to bridge %s.\n", interface_name,
						bridge_name);
					break;

				case ELOOP:
					fprintf(stderr, "device %s is a bridge device itself; "
						"can't enslave a bridge device to a bridge device.\n",
						interface_name);
					break;

				default:
					fprintf(stderr, "can't add %s to bridge %s: %s\n",
						interface_name, bridge_name, strerror(e));
				}

        br_shutdown();
        return 0;
}

// virtual_interface_flagが0ならinterfaceの作成
// virtual_interface_flagが1ならvirtual interfaceの設定
int modify_interface(json_t *interfaces_json, int virtual_interface_flag) {
	if (virtual_interface_flag == 0 ) {
		// delete virtual interfaces before remake
		delete_virtual_interface(get_max_index());
	}

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
		req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
		req.ifi.ifi_index = (int)json_number_value(json_object_get(interface_json, "ifIndex"));
		req.ifi.ifi_change =  0xFFFFFFFF;
		req.ifi.ifi_type = (unsigned short)json_number_value(json_object_get(linux_json, "ifi_type"));//0;
		req.ifi.ifi_flags = (unsigned int)json_number_value(json_object_get(linux_json, "ifi_flags"));

		int real_or_virtual = interface_real_or_virtual(interface_json);
		if (virtual_interface_flag == 0) {

			if (real_or_virtual < 0) {
				fprintf(stderr, "Error: can't judge interface is virtual or not\n");
			}

			if (real_or_virtual == 1) { // real
				req.n.nlmsg_type = RTM_SETLINK;
			}
			else { // virtual
				req.n.nlmsg_type = RTM_NEWLINK;
				req.n.nlmsg_flags |= NLM_F_CREATE | NLM_F_EXCL;
			}
		} else {
			req.n.nlmsg_type = RTM_SETLINK;
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
				if (virtual_interface_flag == 0) {
					if (real_or_virtual == 1) {
						continue;
					}
					int len = ll_addr_a2n(abuf, sizeof(abuf), (char *)json_string_value(value));
					if (len < 0) {
						return -1;
					}
					addattr_l(&req.n, sizeof(req), IFLA_ADDRESS, abuf, len);
				}
			}
			if (strcmp(key, "ifType") == 0) {
				char *type = (char *)json_string_value(value);
				struct rtattr *linkinfo;
				linkinfo = addattr_nest(&req.n, sizeof(req), IFLA_LINKINFO);
				addattr_l(&req.n, sizeof(req), IFLA_INFO_KIND, type, strlen(type));
				addattr_nest_end(&req.n, linkinfo);
			}
		}

		if (virtual_interface_flag == 1) {
			const char *linux_key;
			json_t *linux_value;

			json_object_foreach(linux_json, key, value) {
				if (strcmp(key, "bridge") == 0) {
					json_t *slave_json = json_object_get(value, "slave");
					json_t *slave_value;
					size_t slave_index;
					json_array_foreach(slave_json, slave_index, slave_value) {
						br_addif((char *)json_string_value(slave_value), interface_name);
					}
				}
			}
		}

		if (virtual_interface_flag == 0 && real_or_virtual == 1) {
			down_interface(req.ifi.ifi_index);
		} else if (virtual_interface_flag == 1) {
			down_interface(req.ifi.ifi_index);
		}
		fprintf(stderr, "%d: interface \"%s\" message is ready.\n", req.ifi.ifi_index, interface_name);

		if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
			exit(2);
		}

		fprintf(stderr, "%d: interface \"%s\" was changed.\n\n", req.ifi.ifi_index, interface_name);

	}
	return 0;

}

int read_interface_file(json_t* interfaces_json) {

	modify_interface(interfaces_json, 0);
	modify_interface(interfaces_json, 1);

	fprintf(stderr, "Success arranging all interfaces!\n\n");
	return 0;
}
