#ifndef _REVERT_ROUTE_H_
#define _REVERT_ROUTE_H_

#include "lib/iproute/libnetlink.h"
#include <jansson.h>

static inline int rtm_get_table(struct rtmsg *r, struct rtattr **tb)
{
	__u32 table = r->rtm_table;
	if (tb[RTA_TABLE])
		table = *(__u32*) RTA_DATA(tb[RTA_TABLE]);
	return table;
}

int read_route_file(json_t *routes_json);

#endif
