#include <stdio.h>
#include "interface.h"

int main() {
	if (make_interface_file(".negi/negi2.json") < 0) {
		fprintf(stderr, "Error: can't make interface file");
	}

	// read_interface_file(".negi/negi.json");
	return 0;
}