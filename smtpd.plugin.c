#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char * argv[]) {
	int update = 1;

	if (argc > 1)
		update = atoi(argv[1]);

	return 0;
}
