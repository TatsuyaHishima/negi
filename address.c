#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "lib/iproute/libnetlink.h"
#include <jansson.h>


#include "common.h"
#include "address.h"
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

		// Process Data of Netlink Message as ifaddrmsg
		if (ifamsg->ifa_family != AF_INET && ifamsg->ifa_family != AF_INET6) {
			printf("error family: %d\n", ifamsg->ifa_family);
			continue;
		}

		json_t *address_json = json_object();
		json_t *ifaddrmsg_json = json_object();
		json_object_set_new(ifaddrmsg_json, "interface", json_integer(ifamsg->ifa_index));
		// json_object_set_new(ifaddrmsg_json, "prefixlen", json_string((char *)ifamsg->ifa_prefixlen));

		json_object_set_new(address_json, "ifaddrmsg", ifaddrmsg_json);

		// Add Each Parameter to json file
		// printf("index: %d\n", ifamsg->ifa_index);

		// Analyze rtattr Message
		int len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifamsg));;

		struct rtattr *tb[IFA_MAX+1];
		parse_rtattr(tb, IFA_MAX, IFA_RTA(ifamsg), len);

		json_t *rta_json = json_object();


		if (tb[IFA_LABEL]) {
			json_object_set_new(rta_json, "LABEL", json_string((char *)RTA_DATA(tb[IFA_LABEL])));
			// printf("%s\n", (char *)RTA_DATA(tb[IFA_LABEL]));
		}
		else {
			char name[8];
			if_indextoname(ifamsg->ifa_index, name);
			json_object_set_new(rta_json, "LABEL", json_string(name));
			// printf("%s\n", name);
		}

		if (tb[IFA_ADDRESS]) {
			unsigned char *a = RTA_DATA(tb[IFA_ADDRESS]);
			char buf[64];
			if (ifamsg->ifa_family == AF_INET) {
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "ADDRESS", json_string(buf));
				// printf("ADDRESS -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				json_object_set_new(rta_json, "ADDRESS", json_string(buf));
				// printf("ADDRESS -> %s\n", buf);
			}
		}
		if (tb[IFA_LOCAL]) {
			unsigned char *a = RTA_DATA(tb[IFA_LOCAL]);
			char buf[64];
			if (ifamsg->ifa_family == AF_INET) {
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "LOCAL", json_string(buf));
				// printf("LOCAL -> %d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);
				if (tb[IFA_ADDRESS] == NULL || memcmp(RTA_DATA(tb[IFA_ADDRESS]), RTA_DATA(tb[IFA_LOCAL]), 4) == 0) {
					// printf("netmask: /%d ", ifamsg->ifa_prefixlen);
					json_object_set_new(rta_json, "prefixlen", json_integer(ifamsg->ifa_prefixlen));
				}

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				json_object_set_new(rta_json, "LOCAL", json_string(buf));
				// printf("LOCAL -> %s\n", buf);
			}
		}
		if (tb[IFA_BROADCAST]) {
			unsigned char *a = RTA_DATA(tb[IFA_BROADCAST]);
			char buf[64];
			if (ifamsg->ifa_family == AF_INET) {
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "BROADCAST", json_string(buf));
				// printf("BROADCAST ->%d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
				json_object_set_new(rta_json, "BROADCAST", json_string(buf));
				// printf("BROADCAST -> %s\n", buf);
			}
		}
		if (tb[IFA_ANYCAST]) {
			unsigned char *a = RTA_DATA(tb[IFA_ANYCAST]);
			char buf[64];
			if (ifamsg->ifa_family == AF_INET) {
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "ANYCAST", json_string(buf));				
				// printf("ANYCAST ->%d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
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
			unsigned char *a = RTA_DATA(tb[IFA_MULTICAST]);
			char buf[64];
			if (ifamsg->ifa_family == AF_INET) {
				sprintf(buf, "%d:%d:%d:%d", a[0], a[1], a[2], a[3]);
				json_object_set_new(rta_json, "MULTICAST", json_string(buf));
				// printf("MULTICAST ->%d.%d.%d.%d\n", a[0], a[1], a[2], a[3]);

			}
			else if (ifamsg->ifa_family == AF_INET6) {
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
	// printf("%s", json_data);

	make_file(filename, json_data);

	free(a);

	rtnl_close(&rth);

	return 0;
}

int link_local(char *address) {
	if (strncmp(address, "fe80:", 5) == 0) {
		return 1;
	}
	return 0;
}

int delete_address(int interface, char *address) {

	if (link_local(address)) {
		fprintf(stderr, "Can't delete ipv6 link local address.\n");
		return 0;
	}

	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	struct iplink_req {
		struct nlmsghdr     n;
		struct ifaddrmsg    ifa;
		char            buf[256];
	};

	struct iplink_req req;

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.ifa.ifa_index = interface;
	req.ifa.ifa_flags = 0;
	req.ifa.ifa_scope = 0;
	req.n.nlmsg_type = RTM_DELADDR;

	inet_prefix lcl;
	get_prefix(&lcl, address, req.ifa.ifa_family);
	addattr_l(&req.n, sizeof(req), IFA_LOCAL, &lcl.data, lcl.bytelen);
	req.ifa.ifa_prefixlen = lcl.bitlen;
	req.ifa.ifa_family = lcl.family;

	ll_init_map(&rth);

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
		exit(2);
	}

	printf("delete address %s\n", address);

	return 0;
}

int delete_all_address() {
	struct rtnl_handle rth = { .fd = -1 };

	if (rtnl_open(&rth, 0) < 0) {
		exit(1);
	}

	int preferred_family = AF_PACKET;

	if (rtnl_wilddump_request(&rth, preferred_family, RTM_GETADDR) < 0) {
		perror("Cannot send dump request\n");
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
		struct ifaddrmsg *ifamsg = NLMSG_DATA(nlhdr);

		// Process Data of Netlink Message as ifaddrmsg
		if (ifamsg->ifa_family != AF_INET && ifamsg->ifa_family != AF_INET6) {
			printf("error family: %d\n", ifamsg->ifa_family);
			continue;
		}

		// Analyze rtattr Message
		int len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifamsg));;
		printf("%d\n", ifamsg->ifa_index);

		struct rtattr *tb[IFA_MAX+1];
		parse_rtattr(tb, IFA_MAX, IFA_RTA(ifamsg), len);
		char buf[64] = "";
		if (tb[IFA_ADDRESS]) {
			unsigned char *a = RTA_DATA(tb[IFA_ADDRESS]);
			if (ifamsg->ifa_family == AF_INET) {
				sprintf(buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
			}
			else if (ifamsg->ifa_family == AF_INET6) {
				inet_ntop(AF_INET6, a, buf, sizeof(buf));
			}
		}
		delete_address(ifamsg->ifa_index, buf);
	}
	free(a);
	rtnl_close(&rth);
	return 0;
}

int read_address_file(char* filename) {

	json_error_t error;
	json_t *address_json = json_load_file(filename , JSON_DECODE_ANY, &error);

	if(!address_json) {
		fprintf(stderr, "Error: can't read json file.\n");
		return -1;
	}

	// 全部のアドレス情報を削除する必要あり
	delete_all_address();

	json_t *IFA_json = json_object_get(address_json, "IFA");
	int i;
	for (i = 0; i < (int)json_array_size(IFA_json); i++) {
		json_t *each_address_data = json_array_get(IFA_json, i);
		json_t *ifaddrmsg_json = json_object_get(each_address_data, "ifaddrmsg");
		json_t *rta_json = json_object_get(each_address_data, "rta");

		// print json
		char *json_data = json_dumps(ifaddrmsg_json, JSON_INDENT(4));
		printf("%s\n", json_data);
		free(json_data);

		struct rtnl_handle rth = { .fd = -1 };

		if (rtnl_open(&rth, 0) < 0) {
			exit(1);
		}

		struct iplink_req {
			struct nlmsghdr     n;
			struct ifaddrmsg    ifa;
			char            buf[256];
		};

		struct iplink_req req;

		memset(&req, 0, sizeof(req));
		req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
		req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
		req.ifa.ifa_index = (int)json_number_value(json_object_get(ifaddrmsg_json, "interface"));

		// req.ifa.ifa_prefixlen = 32; // AF_INET6なら64
		// req.ifa.ifa_family = AF_INET; // AF_INET6

		req.ifa.ifa_flags = 0;
		req.ifa.ifa_family = 0;
		req.ifa.ifa_scope = 0;
		req.n.nlmsg_type = RTM_NEWADDR;

		// printf("nlmsg_len: %d\n",req.n.nlmsg_len);
		// printf("nlmsg_flags: %d\n",req.n.nlmsg_flags);
		// printf("nlmsg_type: %d\n",req.n.nlmsg_type);
		// printf("ifa_family: %d\n",req.ifa.ifa_family);
		// printf("ifa_flags: %d\n",req.ifa.ifa_flags);
		// printf("ifa_prefixlen: %d\n",req.ifa.ifa_prefixlen);
		// printf("ifa_scope: %d\n",req.ifa.ifa_scope);
		// printf("ifa_index: %d\n",req.ifa.ifa_index);
		// printf("nlmsg_seq: %d\n",req.n.nlmsg_seq);
		// printf("nlmsg_pid: %d\n",req.n.nlmsg_pid);

		inet_prefix lcl;
		int link_local_flag = 0;
		int prefixlen_flag = 0;
		const char *key;
		json_t *value;
		json_object_foreach(rta_json, key, value) {
			printf("%s\n", key);
			if (strcmp(key, "LOCAL") == 0) {
				get_prefix(&lcl, (char *)json_string_value(value), req.ifa.ifa_family);
				addattr_l(&req.n, sizeof(req), IFA_LOCAL, &lcl.data, lcl.bytelen);
				if (prefixlen_flag == 0) {
					req.ifa.ifa_prefixlen = lcl.bitlen; //32; // AF_INET6なら64
				}
				req.ifa.ifa_family = lcl.family; // AF_INET6
			}

			if (strcmp(key, "ADDRESS") == 0) {
				if (link_local((char *)json_string_value(value))) {
					link_local_flag = 1;
				}
				get_prefix(&lcl, (char *)json_string_value(value), req.ifa.ifa_family);
				addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &lcl.data, lcl.bytelen);
				if (prefixlen_flag == 0) {
					req.ifa.ifa_prefixlen = lcl.bitlen; //32; // AF_INET6なら64
				}
				req.ifa.ifa_family = lcl.family; // AF_INET6
				printf("lcl.family: %d\n",lcl.family);
			}
			if (strcmp(key, "BROADCAST") == 0) {
				inet_prefix addr;
				get_addr(&addr, (char *)json_string_value(value), req.ifa.ifa_family);
				if (req.ifa.ifa_family == AF_UNSPEC)
					req.ifa.ifa_family = addr.family;
				addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &addr.data, addr.bytelen);
			}

			if (strcmp(key, "prefixlen") == 0) {
				printf("prefixlen: %d\n", (int)json_integer_value(value));
				req.ifa.ifa_prefixlen = (int)json_integer_value(value);
				prefixlen_flag = 1;
			}
		}

		if (link_local_flag) {
			fprintf(stderr, "Don't assign link local address.\n");
			continue;
		}

		if (req.ifa.ifa_family == 0) {
			printf("No address data\n");
			exit(5);
		}

		if (req.ifa.ifa_family == AF_INET) {

		}

		ll_init_map(&rth);

		if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
			exit(2);
		}
		printf("one end\n");

	}
	fprintf(stderr, "Success arranging all address!\n\n");

	return 0;
}
