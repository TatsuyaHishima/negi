#ifndef _COMMON_H_
#define _COMMON_H_


#include "lib/iproute/libnetlink.h"


int make_json_file(char *filename, json_t *json);
int rtnl_talkE(struct rtnl_handle *rtnl, struct nlmsghdr *n, pid_t peer, unsigned groups, struct nlmsghdr *answer, rtnl_filter_t junk, void *jarg);

struct nlmsg_list
{
	struct nlmsg_list *next;
	struct nlmsghdr	  h;
};


#endif

#ifndef	INFINITY_LIFE_TIME
#define     INFINITY_LIFE_TIME      0xFFFFFFFFU
#endif
