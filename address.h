#ifndef _ADDRESS_H_
#define _ADDRESS_H_

#include <jansson.h>

json_t* make_address_file(char *filename);
int read_address_file(char *filename);

#endif
