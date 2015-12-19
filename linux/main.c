#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "interface.h"
#include "address.h"
#include "route.h"
#include "common.h"
#include <jansson.h>

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: select \"commit\" or \"revert\"\n");
		return -1;
	}

	argc--;
	argv++;

	if (argc) {
		if (strcmp("commit", *argv) == 0) {
			argc--;
			argv++;

			if (!argc) {
				printf("Usage: negi commit [filepath] [machine name].\n");
				exit(2);
			}

			char *filepath= *argv;
			argc--;
			argv++;

			if (!argc) {
				printf("Usage: negi commit [filepath] [machine name].\n");
				exit(2);
			}

			char *machine_name = *argv;
			argc--;
			argv++;

			json_t *linux_json = json_object();

			json_t *interface_json = json_object();
			interface_json = make_interface_file();

			if (json_object_size(interface_json) == 0) {
				fprintf(stderr, "Error: can't make interface file");
			}

			json_t *ip_json = json_object();

			json_t *address_json = json_object();
			address_json = make_address_file();

			if (json_object_size(address_json) == 0) {
				fprintf(stderr, "Error: can't make address file");
			}
			json_object_set_new(ip_json, "ipAddrTable", address_json);

			json_t *route_json = json_object();
			route_json = make_route_file();

			if (json_object_size(route_json) == 0) {
				fprintf(stderr, "Error: can't make route file");
			}
			json_object_set_new(ip_json, "ipRouteTable", route_json);

			json_object_set_new(linux_json, "interfaces", interface_json);
			json_object_set_new(linux_json, "ip", ip_json);

			char buf[2048];
			sprintf(buf, "%s/%s.json", filepath, machine_name);
			make_json_file(buf, linux_json);

		}
		else if (strcmp("revert", *argv) == 0) {
			argc--;
			argv++;

			if (!argc) {
				printf("Usage: negi revert [/path/to/json]\n");
				exit(2);
			}

			char *filename = *argv;
			json_error_t error;
			json_t *linux_json = json_load_file(filename , JSON_DECODE_ANY, &error);

			if(!linux_json) {
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
		}
	}
	return 0;
}
