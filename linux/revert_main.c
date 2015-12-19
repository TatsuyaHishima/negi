#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "revert_interface.h"
#include "revert_address.h"
#include "revert_route.h"
#include "common.h"
#include <jansson.h>

int main(int argc, char **argv) {
    argc--;
    argv++;

    if (!argc) {
        printf("Usage: negi revert [/path/to/json]\n");
        exit(2);
    }

    char        *filename = *argv;
    json_error_t error;
    json_t      *linux_json = json_load_file(filename, JSON_DECODE_ANY, &error);

    if (!linux_json) {
        fprintf(stderr, "Error: can't read json file.\n");

        return -1;
    }

    json_t *interface_json = json_object_get(linux_json, "interfaces");
    read_interface_file(interface_json);

    json_t *ip_json = json_object_get(linux_json, "ip");

    json_t *address_json = json_object_get(ip_json, "ipAddrTable");
    read_address_file(address_json);

    json_t *route_json = json_object_get(ip_json, "ipRouteTable");
    read_route_file(route_json);

    return 0;
}
