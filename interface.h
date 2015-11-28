#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <jansson.h>

json_t* make_interface_file();
int read_interface_file(char *filename);

#endif
