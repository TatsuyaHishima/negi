#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "revert_interface.h"
#include "revert_address.h"
#include "revert_route.h"
#include "common.h"
#include <jansson.h>

void write_forward(char *forward_num) {
  FILE *file;
	file = fopen("/proc/sys/net/ipv4/ip_forward","w");
	fprintf(file,"%s\n", forward_num);
	fclose(file);
	return 0;
}

int main(int argc, char **argv) {
    argc--;
    argv++;

    if (!argc) {
        printf("Usage: negi_linux_revert logdirPath machineName\n");
        exit(2);
    }
    char        *log_dir_path = *argv;

    argc--;
    argv++;

    if (!argc) {
        printf("Usage: negi_linux_revert logdirPath machineName\n");
        exit(2);
    }
    char        *machine_name = *argv;
    char *filename[2048];
    sprintf(filename, "%s/%s.json", log_dir_path, machine_name);

    json_error_t error;
    json_t      *linux_json = json_load_file(filename, JSON_DECODE_ANY, &error);

    if (!linux_json) {
        fprintf(stderr, "Error: can't read json file.\n");

        return -1;
    }

    json_t *interface_json = json_object_get(linux_json, "interfaces");
    read_interface_file(interface_json);

    json_t *ip_json = json_object_get(linux_json, "ip");

    json_t *forward = json_object_get(ip_json, "ipForwarding");
    write_forward(json_string_value(forward));

    json_t *address_json = json_object_get(ip_json, "ipAddrTable");
    read_address_file(address_json);

    json_t *route_json = json_object_get(ip_json, "ipRouteTable");
    read_route_file(route_json);

    return 0;
}
