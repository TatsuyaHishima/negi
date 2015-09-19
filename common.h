#ifndef _COMMON_H_
#define _COMMON_H_


#include "lib/iproute/libnetlink.h"


int make_file(char *filename, char *string);

struct nlmsg_list
{
	struct nlmsg_list *next;
	struct nlmsghdr	  h;
};

#endif

#ifndef	INFINITY_LIFE_TIME
#define     INFINITY_LIFE_TIME      0xFFFFFFFFU
#endif
