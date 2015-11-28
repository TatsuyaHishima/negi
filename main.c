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
			json_t *linux_json = json_object();

			json_t *interface_json = json_object();
			interface_json = make_interface_file(".negi/negi4.json");

			if (json_object_size(interface_json) == 0) {
				fprintf(stderr, "Error: can't make interface file");
			}

			// print json
			char *json_data = json_dumps(interface_json, JSON_INDENT(4));
			sprintf(json_data, "%s\n", json_data); // add new line to end of file
			printf("%s", json_data);

			make_file(".negi/negi8.json", json_data);

			// json_t *address_json = json_object();
			// if (make_address_file(".negi/negi6.json") < 0) {
			// 	fprintf(stderr, "Error: can't make address file");
			// }

			// json_t *route_json = json_object();
			// if (make_route_file(".negi/negi5.json") < 0) {
			// 	fprintf(stderr, "Error: can't make route file");
			// }
		}
		else if (strcmp("revert", *argv) == 0) {
			read_interface_file(".negi/negi8.json");
			// read_address_file(".negi/negi6.json");
			// read_route_file(".negi/negi5.json");
		}
	}
	return 0;
}