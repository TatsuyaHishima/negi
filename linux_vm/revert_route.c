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
#include "revert_route.h"
#include "lib/utils.h"

struct iplink_req {
    struct nlmsghdr n;
    struct rtmsg    rtm;
    char            buf[1024];
};

static int store_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
    struct nlmsg_list **linfo = (struct nlmsg_list **) arg;
    struct nlmsg_list  *h;
    struct nlmsg_list **lp;

    h = malloc(n->nlmsg_len + sizeof(void *));
    if (h == NULL) {
        return -1;
    }

    memcpy(&h->h, n, n->nlmsg_len);
    h->next = NULL;

    for (lp = linfo; *lp; lp = &(*lp)->next) { /* NOTHING */ }
    *lp = h;

    return 0;
}

int calc_host_len(struct rtmsg *r)
{
    if (r->rtm_family == AF_INET6) {
        return 128;
    }
    else if (r->rtm_family == AF_INET) {
        return 32;
    }
    else if (r->rtm_family == AF_DECnet) {
        return 16;
    }
    else if (r->rtm_family == AF_IPX) {
        return 80;
    }
    else {
        return -1;
    }
}

int delete_route(char *address, int netmask)
{
    struct rtnl_handle rth = { .fd = -1 };

    if (rtnl_open(&rth, 0) < 0) {
        exit(1);
    }

    struct iplink_req req;

    memset(&req, 0, sizeof(req));
    req.n.nlmsg_len    = NLMSG_LENGTH(sizeof(struct rtmsg));
    req.n.nlmsg_flags  = NLM_F_REQUEST | NLM_F_ACK;
    req.n.nlmsg_type   = RTM_DELROUTE;
    req.rtm.rtm_family = AF_UNSPEC;
    req.rtm.rtm_table  = RT_TABLE_MAIN;
    req.rtm.rtm_scope  = RT_SCOPE_NOWHERE;

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
    int              errnum = rtnl_talkE(&rth, &req.n, 0, 0, &answer, NULL, NULL);
    if (errnum < 0) {
        exit(2);
    }

    if (answer) {
        switch (errnum) {
            case 0: // Success
                fprintf(stderr, "delete route to %s\n", address);
                break;

            case 3: // No such device
                // fprintf(stderr, "already deleted.\n");
                break;

            default:
                fprintf(stderr, "ERROR!\terrno: %d\n", errnum);
                perror("Netlink");
                exit(2);
                break;
        }
    } else {
        fprintf(stderr, "Something Wrong!\n");
        exit(2);
    }

    return 0;
}

int delete_all_route()
{
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

        if (nlhdr->nlmsg_type != RTM_NEWROUTE && nlhdr->nlmsg_type != RTM_DELROUTE) {
            fprintf(stderr, "Not a route: %08x %08x %08x\n", nlhdr->nlmsg_len, nlhdr->nlmsg_type, nlhdr->nlmsg_flags);

            return 0;
        }

        struct rtmsg *rtm = NLMSG_DATA(nlhdr);
        int           len = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(*rtm));

        if (len < 0) {
            fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);

            return -1;
        }

        // Analyze rtattr Message
        struct rtattr *tb[RTA_MAX + 1];
        parse_rtattr(tb, RTA_MAX, RTM_RTA(rtm), len);

        char dst_address[64] = "";
        char abuf[256];
        int  host_len = calc_host_len(rtm);

        if (tb[RTA_DST]) {
            if (rtm->rtm_dst_len != host_len) {
                sprintf(dst_address, "%s/%d", rt_addr_n2a(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_DST]), RTA_DATA(tb[RTA_DST]), abuf, sizeof(abuf)), rtm->rtm_dst_len);
            } else {
                sprintf(dst_address, "%s", format_host(rtm->rtm_family, RTA_PAYLOAD(tb[RTA_DST]), RTA_DATA(tb[RTA_DST]), abuf, sizeof(abuf)));
            }
        } else if (rtm->rtm_dst_len) {
            sprintf(dst_address, "0/%d", rtm->rtm_dst_len);
        } else {
            sprintf(dst_address, "default");
        }

        if (strncmp(dst_address, "127.0.0.0", 9) != 0 && strcmp(dst_address, "ff00::") != 0) {
            delete_route((char *) dst_address, rtm->rtm_dst_len);
        }
    }

    printf("delete all routes.\n\n");
    free(r);
    rtnl_close(&rth);

    return 0;
}

// network is unreachableを回避するため、全経路を登録し終えるまでwhileループを回す
void modify_route(json_t *ipRouteEntry_json, int default_flag)
{
    int end = 0;
    while (end == 0) {
				end = 1;
        int i;
        for (i = 0; i < (int) json_array_size(ipRouteEntry_json); i++) {
            json_t *route_json = json_array_get(ipRouteEntry_json, i);
            json_t *linux_json = json_object_get(route_json, "linux");

            struct rtnl_handle rth = { .fd = -1 };

            if (rtnl_open(&rth, 0) < 0) {
                exit(1);
            }

            struct iplink_req req;

            memset(&req, 0, sizeof(req));
            req.n.nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtmsg));
            req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
            req.n.nlmsg_type  = RTM_NEWROUTE;

            req.rtm.rtm_family = AF_UNSPEC;
            req.rtm.rtm_type   = (int) json_number_value(json_object_get(route_json, "ipRouteType"));
            if (req.rtm.rtm_type >= 5) { // 5以降は定義されていない
                req.rtm.rtm_type = 1;
            }
            req.rtm.rtm_protocol = (int) json_number_value(json_object_get(route_json, "ipRouteProto"));
            req.rtm.rtm_scope    = (int) json_number_value(json_object_get(linux_json, "rtm_scope"));
            req.rtm.rtm_table    = (int) json_number_value(json_object_get(linux_json, "rtm_table"));
            req.rtm.rtm_dst_len  = (int) json_number_value(json_object_get(route_json, "ipRouteMask"));

            char       *route_name = "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff";
            const char *key;
            json_t     *value;
            json_object_foreach(route_json, key, value) {
                if (strcmp(key, "ipRouteDest") == 0) {
                    inet_prefix dst;
                    route_name = (char *) json_string_value(value);
                    get_prefix(&dst, route_name, req.rtm.rtm_family);
                    if (req.rtm.rtm_family == AF_UNSPEC) {
                        req.rtm.rtm_family = dst.family;
                    }
                    if (dst.bytelen) {
                        addattr_l(&req.n, sizeof(req), RTA_DST, &dst.data, dst.bytelen);
                    }
                }

                if (strcmp(key, "ipRouteIfIndex") == 0) {
                    addattr32(&req.n, sizeof(req), RTA_OIF, json_integer_value(value));
                }

                if (strcmp(key, "ipRouteNextHop") == 0) {
                    inet_prefix addr;
                    get_addr(&addr, (char *) json_string_value(value), req.rtm.rtm_family);
                    if (req.rtm.rtm_family == AF_UNSPEC) {
                        req.rtm.rtm_family = addr.family;
                    }
                    addattr_l(&req.n, sizeof(req), RTA_GATEWAY, &addr.data, addr.bytelen);
                }

                if (strcmp(key, "ipRouteMetric1") == 0) {
                    addattr32(&req.n, sizeof(req), RTA_PRIORITY, json_integer_value(value));
                }

                if (strcmp(key, "ipRouteInfo") == 0) {
                    inet_prefix addr;
                    get_addr(&addr, (char *) json_string_value(value), req.rtm.rtm_family);
                    if (req.rtm.rtm_family == AF_UNSPEC) {
                        req.rtm.rtm_family = addr.family;
                    }
                    addattr_l(&req.n, sizeof(req), RTA_PREFSRC, &addr.data, addr.bytelen);
                }
            }

            if (skip_flag == 1) {
                continue;
            }
            ll_init_map(&rth);

            if (req.rtm.rtm_family == AF_UNSPEC) {
                req.rtm.rtm_family = AF_INET;
            }

            struct nlmsghdr *answer;
            int              errnum = rtnl_talkE(&rth, &req.n, 0, 0, &answer, NULL, NULL);
            if (errnum < 0) {
                exit(2);
            }

            if (answer) {
                switch (errnum) {
                    case 0: // Success
                        fprintf(stderr, "arrange route to %s/%d\n", route_name, req.rtm.rtm_dst_len);
                        break;

                    case 17: // File exists
                        // fprintf(stderr, "route already exists.\n");
                        break;

                    case 19: // No such device
                        // fprintf(stderr, "No such device");
                        break;

										case 101: // Network is unreachable
												end = 0;
												break;

                    default:
                        fprintf(stderr, "ERROR!\terrno: %d\n", errnum);
                        perror("Netlink");
                        exit(2);
                        break;
                }
            } else {
                fprintf(stderr, "Something Wrong!\n");
                exit(2);
            }
        }
    }
}

int read_route_file(json_t *routes_json)
{
    // routeの削除
    delete_all_route();

    json_t *ipRouteEntry_json = json_object_get(routes_json, "ipRouteEntry");

    // defaultへの経路はGatewayへの経路が登録されてからでないとnetwork is unreachableになるので
    // defaultへの経路とそうでないものを分ける
    modify_route(ipRouteEntry_json, 0);
    modify_route(ipRouteEntry_json, 1);
    fprintf(stderr, "Success arranging all routes!\n\n");

    return 0;
}
