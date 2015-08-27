#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>
#include <linux/if_link.h>
// #include <linux/netdevice.h> // for lt Linux 2.4
#include <jansson.h>
#include "json.h"

int make_interface_file(char *filename) {
	json_t *interface_json = json_object();
	int soc;
	struct sockaddr_nl sa;
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifi;
	} request;
	int seq = 100;
	int n;

	char buf[4096];
	struct nlmsghdr *nlhdr;

	// Make Netlink Socket
	soc = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);

	// Preparing sockaddr_nl
	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK; // use netlink
	sa.nl_pid = 0; // kernel
	sa.nl_groups = 0; // unicast

	// Make Request Message to Kernel
	memset(&request, 0, sizeof(request));

	request.nh.nlmsg_len = sizeof(request);
	request.nh.nlmsg_type = RTM_GETLINK;
	request.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	request.nh.nlmsg_pid = 0; // kernel
	request.nh.nlmsg_seq = seq; // need to hack
	// request.ifi.ifi_family = ARPHRD_ETHER;

	// Send Netlink Message to Kernel
	n = sendto(soc, (void *)&request, sizeof(request), 0, (struct sockaddr *)&sa, sizeof(sa));
	if (n < 0) {
		perror("sendto");
		return 1;
	}

	// Receive Message from Kernel
	n = recv(soc, buf, sizeof(buf), 0);
	if (n < 0) {
		perror("recvmsg");
		return 1;
	}

	// printf("recv\n");

	// Analyze Netlink Message
	int end = 0;
	while (!end) {
		for (nlhdr = (struct nlmsghdr *)buf; NLMSG_OK(nlhdr, n); nlhdr = NLMSG_NEXT(nlhdr, n)) {
			switch (nlhdr->nlmsg_type) {
				case 3: // NLMSG_DONE
				end++;
				break;
				case 16: // RTM_NEWLINK
				end = 0;
				struct ifinfomsg *ifimsg;
				struct rtattr *rta;
				int rtalist_len;

		// Print Netlink Message length and type
				printf("===\n");
				printf("len: %d\n", nlhdr->nlmsg_len);
				printf("type: %d\n", nlhdr->nlmsg_type);

		// Link State Information Type is RTM_NEWLINK
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

		// Add Each Parameter to json file

				json_t *ifinfomsg_json = json_object();
				json_object_set_new(ifinfomsg_json, "family", json_integer(ifimsg->ifi_family));
				json_object_set_new(ifinfomsg_json, "type", json_integer(ifimsg->ifi_type));
				json_object_set_new(ifinfomsg_json, "index", json_integer(ifimsg->ifi_index));
				json_object_set_new(ifinfomsg_json, "flags", json_integer(ifimsg->ifi_flags));
				json_object_set_new(ifinfomsg_json, "change", json_integer(ifimsg->ifi_change));

				json_object_set_new(interface_json, "ifinfomsg", ifinfomsg_json);


		// Analyze rtattr Message
				rtalist_len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(struct ifinfomsg));
				json_t *rta_json = json_object();

				for (rta = IFLA_RTA(ifimsg); RTA_OK(rta, rtalist_len); rta = RTA_NEXT(rta, rtalist_len)) {
					printf("	type: %d\n", rta->rta_type);

					switch (rta->rta_type) {
						case IFLA_IFNAME:
						json_object_set_new(rta_json, "ifname", json_string((char *)RTA_DATA(rta)));
						printf("		+ %s\n", (char *)RTA_DATA(rta));
						break;

						case IFLA_MTU:
						json_object_set_new(rta_json, "mtu", json_integer(*(int *)RTA_DATA(rta)));
						printf("		+ MTU: %d\n", *(int *)RTA_DATA(rta));
						break;

						case IFLA_LINK:
						json_object_set_new(rta_json, "link", json_integer(*(int *)RTA_DATA(rta)));				
						printf("		+ Link Type: %d\n", *(int *)RTA_DATA(rta));
						break;

						case IFLA_ADDRESS:
						case IFLA_BROADCAST:
						{
							unsigned char *a = RTA_DATA(rta);

							if (RTA_PAYLOAD(rta) == 6) {
								char buf[64];
								sprintf(buf, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", a[0], a[1], a[2], a[3], a[4], a[5]);
								json_object_set_new(rta_json, rta->rta_type == IFLA_ADDRESS ? "address" : "broadcast", json_string(buf));
								printf("		+ %s: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", rta->rta_type == IFLA_ADDRESS ? "Address" : "Broadcast", a[0], a[1], a[2], a[3], a[4], a[5]);
							}
						}
						break;

						case IFLA_STATS:
						{
							struct rtnl_link_stats *nds = RTA_DATA(rta);
							if (RTA_PAYLOAD(rta) != (int)sizeof(struct rtnl_link_stats)) {
								printf("SOMETHING WRONG!\n");
								break;
							}
							json_object_set_new(rta_json, "rxpkt", json_integer((long)nds->rx_packets));
							json_object_set_new(rta_json, "txpkt", json_integer((long)nds->tx_packets));
							printf("		+ stats: rxpkt = %ld, tkpkt = %ld\n", (long)nds->rx_packets, (long)nds->tx_packets);
						}
						break;

						default:
						printf("		+ other type: %d\n", rta->rta_type);
						break;
					}
				}
				json_object_set_new(interface_json, "rta", rta_json);

		// print json
				char *json_data = json_dumps(interface_json, JSON_INDENT(4));
				printf("%s\n", json_data);
				free(json_data);		
				break;
				default:
				printf("message type %d, length %d\n", nlhdr->nlmsg_type, nlhdr->nlmsg_len);
				break;
			}
		}

	}
	close(soc);

	return 0;
}