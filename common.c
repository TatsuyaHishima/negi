#include <stdio.h>

#include <jansson.h>
#include "common.h"


int make_file(char *filename, char *string) {
	FILE *fp;

	fp = fopen( filename, "w" );
	if( fp == NULL ){
		printf( "%s couldn't open\n", filename );
		return -1;
	}

	fputs(string, fp);

	fclose(fp);

	printf( "made \"%s\"\n", filename );
	return 0;
}

