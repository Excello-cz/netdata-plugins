#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char * argv[]) {
	const char * file_name;
	const char * argv0;
	int update = 1;

	argv0 = *argv; argv++; argc--;

	if (argc >= 1) {
		update = atoi(*argv);
		argv++; argc--;
	}

	if (argc) {
		file_name = *argv;
		argv++; argc--;
	}

	return 0;
}
