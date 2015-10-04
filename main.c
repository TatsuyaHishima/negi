#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "interface.h"
#include "address.h"
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
			argv++;
			char filename[100] = ".negi/negi";

		    time_t timer;
		    struct tm *date;
		    char str[256];

		    timer = time(NULL); /* 経過時間を取得 */
			date = localtime(&timer); /* 経過時間を時間を表す構造体 date に変換 */
			strftime(str, 255, "%H%M%S", date);
			// printf("%s\n", str);

			strcat(filename, str);
			strcat(filename, ".json");
			// printf("%s\n", filename);
			if (make_interface_file(filename) < 0) {
				fprintf(stderr, "Error: can't make interface file");
			}
			// if (make_address_file(".negi/negi5.json") < 0) {
			// 	fprintf(stderr, "Error: can't make interface file");
			// }
		}
		else if (strcmp("revert", *argv) == 0) {
			argv++;
			// printf("filename = %s\n", *(argv));
			char filename[100] = ".negi/";
			strcat(filename, *(argv));
			// printf("%s\n", filename);
			read_interface_file(filename);
		}
	}
	return 0;
}