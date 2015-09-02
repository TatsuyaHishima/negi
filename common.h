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
