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
// #include <linux/netdevice.h> // for lt Linux 2.4
#include <jansson.h>

#define IFLIST_REPLY_BUFFER 8096

struct iplink_req {
	struct nlmsghdr n;
	struct ifinfomsg i;
	char buf[1024];
};

int make_file(char *filename, char *string) {
	FILE *fp;

	fp = fopen( filename, "w" );
	if( fp == NULL ){
		printf( "%s couldn't open\n", filename );
		return -1;
	}

	fputs(string, fp);

	fclose(fp);

	printf( "made \"%s\"\n", filename );
	return 0;
}

struct nlmsg_list
{
	struct nlmsg_list *next;
	struct nlmsghdr	  h;
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

int make_interface_file(char *filename) {
	json_t *IFLA_json = json_object();
	json_t *interface_array = json_array();

	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	int preferred_family = AF_PACKET;

	if (rtnl_wilddump_request(&rth, preferred_family, RTM_GETLINK) < 0) {
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

		// // Print Netlink Message length and type
		// printf("len: %d\n", nlhdr->nlmsg_len);
		// printf("type: %d\n", nlhdr->nlmsg_type);

		struct ifinfomsg *ifimsg = NLMSG_DATA(nlhdr);

		// Process Data of Netlink Message as ifinfomsg
		if (ifimsg->ifi_family != AF_UNSPEC && ifimsg->ifi_family != AF_INET6) {
			printf("error family: %d\n", ifimsg->ifi_family);
			continue;
		}

		// Add Each Parameter to json file
		json_t *interface_json = json_object();	
		json_t *ifinfomsg_json = json_object();
		// printf("%d\n", ifimsg->ifi_index);
		json_object_set_new(ifinfomsg_json, "family", json_integer(ifimsg->ifi_family));
		json_object_set_new(ifinfomsg_json, "type", json_integer(ifimsg->ifi_type));
		json_object_set_new(ifinfomsg_json, "index", json_integer(ifimsg->ifi_index));
		json_object_set_new(ifinfomsg_json, "flags", json_integer(ifimsg->ifi_flags));
		json_object_set_new(ifinfomsg_json, "change", json_integer(ifimsg->ifi_change));

		json_object_set_new(interface_json, "ifinfomsg", ifinfomsg_json);

		// Analyze rtattr Message
		int len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifimsg));;

		struct rtattr *tb[IFLA_MAX+1];
		parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifimsg), len);

		json_t *rta_json = json_object();

		if (tb[IFLA_IFNAME]) {
			// printf("\n%s\n", (char *)RTA_DATA(tb[IFLA_IFNAME]));
			json_object_set_new(rta_json, "IFNAME", json_string((char *)RTA_DATA(tb[IFLA_IFNAME])));
		}

		if (tb[IFLA_LINK]) {
			// printf("%s\n", (char *)RTA_DATA(tb[IFLA_LINK]));
			json_object_set_new(rta_json, "LINK", json_integer(*(int *)RTA_DATA(tb[IFLA_LINK])));
		}

		if (tb[IFLA_MTU]) {
			// printf("%d\n", *(int *)RTA_DATA(tb[IFLA_MTU]));
			json_object_set_new(rta_json, "MTU", json_integer(*(int *)RTA_DATA(tb[IFLA_MTU])));
		}

		if (tb[IFLA_QDISC]) {
			// printf("%s\n", (char *)RTA_DATA(tb[IFLA_QDISC]));
			json_object_set_new(rta_json, "QDISC", json_string((char *)RTA_DATA(tb[IFLA_QDISC])));
		}

		if (tb[IFLA_MASTER]) {
			// printf("%d\n", *(int *)RTA_DATA(tb[IFLA_MASTER]));
			json_object_set_new(rta_json, "MASTER", json_integer(*(int *)RTA_DATA(tb[IFLA_MASTER])));
		}

		static const char *oper_states[] = {
			"UNKNOWN", "NOTPRESENT", "DOWN", "LOWERLAYERDOWN", 
			"TESTING", "DORMANT",	 "UP"
		};

		if (tb[IFLA_OPERSTATE]) {
			__u8 state = *(__u8 *)RTA_DATA(tb[IFLA_OPERSTATE]);
			if (state >= sizeof(oper_states)/sizeof(oper_states[0])){
				fprintf(stderr, "state %#x ", state);
			}
			else{
				// printf("state %s ", oper_states[state]);
				json_object_set_new(rta_json, "OPERSTATE", json_string(oper_states[state]));
			}
		}

		if (tb[IFLA_TXQLEN]) {
			// printf("%d\n", *(int *)RTA_DATA(tb[IFLA_TXQLEN]));
			json_object_set_new(rta_json, "TXQLEN", json_integer(*(int *)RTA_DATA(tb[IFLA_TXQLEN])));
		}

		if (tb[IFLA_ADDRESS]) {
			unsigned char *a = RTA_DATA(tb[IFLA_ADDRESS]);

			if (RTA_PAYLOAD(tb[IFLA_ADDRESS]) == 6) {
				char buf[64];
				sprintf(buf, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", a[0], a[1], a[2], a[3], a[4], a[5]);
				json_object_set_new(rta_json, "ADDRESS", json_string(buf));
				// printf("		+ %s: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", "ADDRESS", a[0], a[1], a[2], a[3], a[4], a[5]);

			}
		}

		if (tb[IFLA_BROADCAST]) {
			unsigned char *a = RTA_DATA(tb[IFLA_BROADCAST]);

			if (RTA_PAYLOAD(tb[IFLA_BROADCAST]) == 6) {
				char buf[64];
				sprintf(buf, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", a[0], a[1], a[2], a[3], a[4], a[5]);
				json_object_set_new(rta_json, "BROADCAST", json_string(buf));
				// printf("		+ %s: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", "BROADCAST", a[0], a[1], a[2], a[3], a[4], a[5]);

			}
		}

		struct link_util
		{
			struct link_util	*next;
			const char		*id;
			int			maxattr;
			int			(*parse_opt)(struct link_util *, int, char **,
				struct nlmsghdr *);
			void			(*print_opt)(struct link_util *, FILE *,
				struct rtattr *[]);
			void			(*print_xstats)(struct link_util *, FILE *,
				struct rtattr *);
		};

		if (tb[IFLA_LINKINFO]) {
			struct rtattr *linkinfo[IFLA_INFO_MAX+1];
			// struct link_util *lu;
			char *kind;

			parse_rtattr_nested(linkinfo, IFLA_INFO_MAX, tb[IFLA_LINKINFO]);

			if (!linkinfo[IFLA_INFO_KIND]){
				exit(2);
			}
			kind = RTA_DATA(linkinfo[IFLA_INFO_KIND]);
			// fprintf(stderr, "%s", kind);
			json_object_set_new(rta_json, "INFO_KIND", json_string(kind));

		}

		if (tb[IFLA_IFALIAS]) {
			// printf("%s\n", (char *)RTA_DATA(tb[IFLA_IFALIAS]));
			json_object_set_new(rta_json, "IFALIAS", json_string((const char *)RTA_DATA(tb[IFLA_IFALIAS])));
		}

		if (tb[IFLA_STATS64]) { // あとで
			// char * _SL_ = "\n";
			// struct rtnl_link_stats64 slocal;
			// struct rtnl_link_stats64 *s = RTA_DATA(tb[IFLA_STATS64]);
			// if (((unsigned long)s) & (sizeof(unsigned long)-1)) {
			// 	memcpy(&slocal, s, sizeof(slocal));
			// 	s = &slocal;
			// }
			// fprintf(stdout, "%s", _SL_);
			// fprintf(stderr, "    RX: bytes  packets  errors  dropped overrun mcast   %s%s",
			// 	s->rx_compressed ? "compressed" : "", _SL_);
			// fprintf(stderr, "    %-10llu %-8llu %-7llu %-7llu %-7llu %-7llu",
			// 	(unsigned long long)s->rx_bytes,
			// 	(unsigned long long)s->rx_packets,
			// 	(unsigned long long)s->rx_errors,
			// 	(unsigned long long)s->rx_dropped,
			// 	(unsigned long long)s->rx_over_errors,
			// 	(unsigned long long)s->multicast);
			// if (s->rx_compressed)
			// 	fprintf(stderr, " %-7llu",
			// 		(unsigned long long)s->rx_compressed);
			// if (1) {
			// 	fprintf(stderr, "%s", _SL_);
			// 	fprintf(stderr, "    RX errors: length  crc     frame   fifo    missed%s", _SL_);
			// 	fprintf(stderr, "               %-7llu  %-7llu %-7llu %-7llu %-7llu",
			// 		(unsigned long long)s->rx_length_errors,
			// 		(unsigned long long)s->rx_crc_errors,
			// 		(unsigned long long)s->rx_frame_errors,
			// 		(unsigned long long)s->rx_fifo_errors,
			// 		(unsigned long long)s->rx_missed_errors);
			// }
			// fprintf(stderr, "%s", _SL_);
			// fprintf(stderr, "    TX: bytes  packets  errors  dropped carrier collsns %s%s",
			// 	s->tx_compressed ? "compressed" : "", _SL_);
			// fprintf(stderr, "    %-10llu %-8llu %-7llu %-7llu %-7llu %-7llu",
			// 	(unsigned long long)s->tx_bytes,
			// 	(unsigned long long)s->tx_packets,
			// 	(unsigned long long)s->tx_errors,
			// 	(unsigned long long)s->tx_dropped,
			// 	(unsigned long long)s->tx_carrier_errors,
			// 	(unsigned long long)s->collisions);
			// if (s->tx_compressed)
			// 	fprintf(stderr, " %-7llu",
			// 		(unsigned long long)s->tx_compressed);
			// if (1) {
			// 	fprintf(stderr, "%s", _SL_);
			// 	fprintf(stderr, "    TX errors: aborted fifo    window  heartbeat%s", _SL_);
			// 	fprintf(stderr, "               %-7llu  %-7llu %-7llu %-7llu",
			// 		(unsigned long long)s->tx_aborted_errors,
			// 		(unsigned long long)s->tx_fifo_errors,
			// 		(unsigned long long)s->tx_window_errors,
			// 		(unsigned long long)s->tx_heartbeat_errors);
			// }
		}
		if (!tb[IFLA_STATS64] && tb[IFLA_STATS]) { // あとで
			// char * _SL_ = "\n";
			// struct rtnl_link_stats slocal;
			// struct rtnl_link_stats *s = RTA_DATA(tb[IFLA_STATS]);
			// if (((unsigned long)s) & (sizeof(unsigned long)-1)) {
			// 	memcpy(&slocal, s, sizeof(slocal));
			// 	s = &slocal;
			// }
			// fprintf(stderr, "%s", _SL_);
			// fprintf(stderr, "    RX: bytes  packets  errors  dropped overrun mcast   %s%s",
			// 	s->rx_compressed ? "compressed" : "", _SL_);
			// fprintf(stderr, "    %-10u %-8u %-7u %-7u %-7u %-7u",
			// 	s->rx_bytes, s->rx_packets, s->rx_errors,
			// 	s->rx_dropped, s->rx_over_errors,
			// 	s->multicast
			// 	);
			// if (s->rx_compressed)
			// 	fprintf(stderr, " %-7u", s->rx_compressed);
			// if (1) {
			// 	fprintf(stderr, "%s", _SL_);
			// 	fprintf(stderr, "    RX errors: length  crc     frame   fifo    missed%s", _SL_);
			// 	fprintf(stderr, "               %-7u  %-7u %-7u %-7u %-7u",
			// 		s->rx_length_errors,
			// 		s->rx_crc_errors,
			// 		s->rx_frame_errors,
			// 		s->rx_fifo_errors,
			// 		s->rx_missed_errors
			// 		);
			// }
			// fprintf(stderr, "%s", _SL_);
			// fprintf(stderr, "    TX: bytes  packets  errors  dropped carrier collsns %s%s",
			// 	s->tx_compressed ? "compressed" : "", _SL_);
			// fprintf(stderr, "    %-10u %-8u %-7u %-7u %-7u %-7u",
			// 	s->tx_bytes, s->tx_packets, s->tx_errors,
			// 	s->tx_dropped, s->tx_carrier_errors, s->collisions);
			// if (s->tx_compressed)
			// 	fprintf(stderr, " %-7u", s->tx_compressed);
			// if (1) {
			// 	fprintf(stderr, "%s", _SL_);
			// 	fprintf(stderr, "    TX errors: aborted fifo    window  heartbeat%s", _SL_);
			// 	fprintf(stderr, "               %-7u  %-7u %-7u %-7u",
			// 		s->tx_aborted_errors,
			// 		s->tx_fifo_errors,
			// 		s->tx_window_errors,
			// 		s->tx_heartbeat_errors
			// 		);
			// }
		}

		if (tb[IFLA_VFINFO_LIST] && tb[IFLA_NUM_VF]) { // あとで
			// struct rtattr *i, *vflist = tb[IFLA_VFINFO_LIST];
			// int rem = RTA_PAYLOAD(vflist);

			// for (i = RTA_DATA(vflist); RTA_OK(i, rem); i = RTA_NEXT(i, rem)){
			// 	struct rtattr *vfinfo = i;
			// 	struct ifla_vf_mac *vf_mac;
			// 	struct ifla_vf_vlan *vf_vlan;
			// 	struct ifla_vf_tx_rate *vf_tx_rate;
			// 	struct rtattr *vf[IFLA_VF_MAX+1];
			// 	// SPRINT_BUF(b1);

			// 	if (vfinfo->rta_type != IFLA_VF_INFO) {
			// 		fprintf(stderr, "BUG: rta type is %d\n", vfinfo->rta_type);
			// 		return;
			// 	}

			// 	parse_rtattr_nested(vf, IFLA_VF_MAX, vfinfo);

			// 	vf_mac = RTA_DATA(vf[IFLA_VF_MAC]);
			// 	vf_vlan = RTA_DATA(vf[IFLA_VF_VLAN]);
			// 	vf_tx_rate = RTA_DATA(vf[IFLA_VF_TX_RATE]);

			// 	// fprintf(stderr, "\n    vf %d MAC %s", vf_mac->vf, ll_addr_n2a((unsigned char *)&vf_mac->mac, ETH_ALEN, 0, b1, sizeof(b1)));
			// 		if (vf_vlan->vlan)
			// 			fprintf(stderr, ", vlan %d", vf_vlan->vlan);
			// 		if (vf_vlan->qos)
			// 			fprintf(stderr, ", qos %d", vf_vlan->qos);
			// 		if (vf_tx_rate->rate)
			// 			fprintf(stderr, ", tx rate %d (Mbps)", vf_tx_rate->rate);
			// 	}
		}

		json_object_set_new(interface_json, "rta", rta_json);
		json_array_append(interface_array, interface_json);
	}

	json_object_set_new(IFLA_json, "IFLA", interface_array);


	// print json
	char *json_data = json_dumps(IFLA_json, JSON_INDENT(4));
	sprintf(json_data, "%s\n", json_data); // add new line to end of file
	// printf("%s", json_data);

	make_file(filename, json_data);
	free(json_data);
	free(l);

	rtnl_close(&rth);

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
				else if(errmsg->error == -22) { // invalid argument
					continue;
				}
				else if(errmsg->error == 0) { // success
					fprintf(stderr, "Delete interface %2d\n", i);
				}
				else {
					printf("error code: %d, %s\n", errmsg->error, strerror(-errmsg->error));
				}
			}
		}
	}
	printf("Delete all virtual interfaces\n");
	return 0;
}

int get_max_index() {
	int max = 0;
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

				// Process Data of Netlink Message as ifinfomsg
				ifimsg = NLMSG_DATA(nlhdr);
				if (ifimsg->ifi_family != AF_UNSPEC && ifimsg->ifi_family != AF_INET6) {
					printf("error family: %d\n", ifimsg->ifi_family);
					continue;
				}

				// compare with max and interface index number
				if (ifimsg->ifi_index > max) {
					max = ifimsg->ifi_index;
				}
				break;

				default:
				printf("message type %d, length %d\n", nlhdr->nlmsg_type, nlhdr->nlmsg_len);
				break;
			}
		}
	}
	close(soc);

	return max;
}

int interface_real_or_virtual(json_t *ifinfomsg_json) {
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

				// Process Data of Netlink Message as ifinfomsg
				ifimsg = NLMSG_DATA(nlhdr);
				if (ifimsg->ifi_family != AF_UNSPEC && ifimsg->ifi_family != AF_INET6) {
					printf("error family: %d\n", ifimsg->ifi_family);
					continue;
				}


				if (json_object_get(ifinfomsg_json, "index") != NULL) {
					int interface_index = (int)json_number_value(json_object_get(ifinfomsg_json, "index"));
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


int read_interface_file(char* filename) {
	json_error_t error;
	json_t *interface_json = json_load_file(filename , JSON_DECODE_ANY, &error);

	if(!interface_json) {
		fprintf(stderr, "Error: can't read json file.\n");
		return -1;
	}

	// delete virtual interfaces before remake
	// delete_virtual_interface(get_max_index());

	json_t *IFLA_json = json_object_get(interface_json, "IFLA");
	int i;
	for (i = 0; i < (int)json_array_size(IFLA_json); i++) {
		json_t *each_interface_data = json_array_get(IFLA_json, i);
		json_t *ifinfomsg_json = json_object_get(each_interface_data, "ifinfomsg");
		json_t *rta_json = json_object_get(each_interface_data, "rta");

		// print json
		// char *json_data = json_dumps(ifinfomsg_json, JSON_INDENT(4));
		// printf("%s\n", json_data);
		// free(json_data);

		int soc;
		struct sockaddr_nl sa;
		char buf[4096];
		int n;
		int seq = 100;
		struct nlmsghdr *nlhdr;
		struct ifinfomsg *ifihdr;
		struct rtattr *rta;

		soc = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);

		memset(&sa, 0, sizeof(sa));
		sa.nl_family = AF_NETLINK;
		sa.nl_pid = 0; // kernel
		sa.nl_groups = 0;

		memset(buf, 0, sizeof(buf));
		nlhdr = (struct nlmsghdr *)buf;
		ifihdr = NLMSG_DATA(nlhdr);
		nlhdr->nlmsg_pid = 0;
		seq++;
		nlhdr->nlmsg_seq = seq;

		ifihdr->ifi_family = AF_UNSPEC;

		// interface_index = virtual or real
		int real_or_virtual = interface_real_or_virtual(ifinfomsg_json);
		// printf("%d\n", real_or_virtual);

		if (real_or_virtual < 0) {
			fprintf(stderr, "Error: can't judge interface is virtual or not\n");
		}

		if (real_or_virtual == 1) { // real
			// configuration

			nlhdr->nlmsg_type = RTM_SETLINK; 
			nlhdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
			ifihdr->ifi_index = (int)json_number_value(json_object_get(ifinfomsg_json, "index")); // lo
			ifihdr->ifi_change =  0xFFFFFFFF;
			ifihdr->ifi_type = 0;
			ifihdr->ifi_flags = (unsigned int)json_number_value(json_object_get(ifinfomsg_json, "flags"));

			int interface_index = ifihdr->ifi_index;

			nlhdr->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));

			const char *key;
			json_t *value;
			json_object_foreach(rta_json, key, value) {

				if (strcmp(key, "ifname") == 0) {
					// IFNAME
					rta = (void *)((char *)nlhdr + nlhdr->nlmsg_len);
					rta->rta_type = IFLA_IFNAME;
					sprintf((char *)RTA_DATA(rta), "%s", json_string_value(value));
					rta->rta_len = RTA_LENGTH(sizeof(rta));

					nlhdr->nlmsg_len += rta->rta_len;
				}
				else if (strcmp(key, "address") == 0) {
					// ADDRESS
					// rta = (void *)((char *)nlhdr + nlhdr->nlmsg_len);
					// rta->rta_type = IFLA_ADDRESS;

					// sprintf((unsigned char *)RTA_DATA(rta), "%s", json_string_value(value));
					// rta->rta_len = RTA_LENGTH(sizeof(rta));

					// nlhdr->nlmsg_len += rta->rta_len;
				}
				else if (strcmp(key, "broadcast") == 0) {
					// BROADCAST
					// rta = (void *)((char *)nlhdr + nlhdr->nlmsg_len);
					// rta->rta_type = IFLA_BROADCAST;

					// sprintf((unsigned char *)RTA_DATA(rta), "%s", json_string_value(value));
					// rta->rta_len = RTA_LENGTH(sizeof(rta));

					// nlhdr->nlmsg_len += rta->rta_len;					
				}
				else if (strcmp(key, "mtu") == 0) {
					// MTU
					rta = (void *)((char *)nlhdr + nlhdr->nlmsg_len);
					rta->rta_type = IFLA_MTU;
					*(int *)RTA_DATA(rta) = json_number_value(value);
					rta->rta_len = RTA_LENGTH(sizeof(rta));

					nlhdr->nlmsg_len += rta->rta_len;
					
				}
				else if (strcmp(key, "rxpkt") == 0) {
					// RX packets			
				}
				else if (strcmp(key, "txpkt") == 0) {
					// TX packets
				}
				else {
					continue;
				}
			};

			// send attribute
			n = sendto(soc, (void *)nlhdr, nlhdr->nlmsg_len, 0, (struct sockaddr *)&sa, sizeof(sa));
			if (n < 0) {
				perror("sendto");
				return 1;
			}

			n = recv(soc, buf, sizeof(buf), 0);
			if (n < 0) {
				perror("recvmsg");
				return 1;
			}

			// analyze responce
			for (nlhdr = (struct nlmsghdr *)buf; NLMSG_OK(nlhdr, n); nlhdr = NLMSG_NEXT(nlhdr, n)) {
				if (nlhdr->nlmsg_type == NLMSG_ERROR) {
					struct nlmsgerr *errmsg;
					errmsg = NLMSG_DATA(nlhdr);
					if (errmsg->error == 0) {
						fprintf(stdout, "Success: Set Interface %d attribute.\n", interface_index);
					}
					else {
						printf("%d, %s\n", errmsg->error, strerror(-errmsg->error));
					}
				}
			}
		}
		else { // virtual
			// 新規作成

		}
	}

	return 0;
}














