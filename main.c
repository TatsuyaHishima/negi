#include <stdio.h>
#include <string.h>
#include "interface.h"

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: select \"make\" or \"read\"\n");
		return -1;
	}

	if (argc == 2) {
		if (strcmp("make", argv[1]) == 0) {
			if (make_interface_file(".negi/negi4.json") < 0) {
				fprintf(stderr, "Error: can't make interface file");
			}
		}
		else if (strcmp("read", argv[1]) == 0) {
			read_interface_file(".negi/negi4.json");
		}
	}
	return 0;
}