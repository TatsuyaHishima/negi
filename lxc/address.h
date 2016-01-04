#ifndef _ADDRESS_H_
#define _ADDRESS_H_

#include <jansson.h>

json_t* make_address_file();
int read_address_file(json_t *addresses_json);

#endif
