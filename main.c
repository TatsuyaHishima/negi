#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "interface.h"
#include "address.h"
#include "route.h"
#include "common.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: select \"make\" or \"read\"\n");
		return -1;
	}

	argc--;
	argv++;

	if (argc) {
		if (strcmp("commit", *argv) == 0) {
			// if (make_interface_file(".negi/negi4.json") < 0) {
			// 	fprintf(stderr, "Error: can't make interface file");
			// }
			// if (make_address_file(".negi/negi6.json") < 0) {
			// 	fprintf(stderr, "Error: can't make address file");
			// }
			if (make_route_file(".negi/negi5.json") < 0) {
				fprintf(stderr, "Error: can't make route file");
			}
		}
		else if (strcmp("revert", *argv) == 0) {
			read_interface_file(".negi/negi4.json");
		}
	}
	return 0;
}