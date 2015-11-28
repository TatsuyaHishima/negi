#ifndef _ROUTE_H_
#define _ROUTE_H_

#include "lib/iproute/libnetlink.h"

static inline int rtm_get_table(struct rtmsg *r, struct rtattr **tb)
{
	__u32 table = r->rtm_table;
	if (tb[RTA_TABLE])
		table = *(__u32*) RTA_DATA(tb[RTA_TABLE]);
	return table;
}

int make_route_file(char *filename);
int read_route_file(char *filename);

#endif
