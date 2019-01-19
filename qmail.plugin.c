#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_PATH "/var/log/qmail"

static
void
usage(const char * name) {
	fprintf(stderr, "usage: %s <timout> [path]\n", name);
}

static
int
is_directory(const char * name) {
	struct stat st;
	int ret;

	ret = stat(name, &st);

	if (ret == -1) {
		return ret;
	}

	return S_ISDIR(st.st_mode);
}

static
void
detect_log_dirs() {
	struct dirent * dir_entry;
	const char * dir_name;
	DIR * dir;

	dir = opendir(".");
	if (dir == NULL) {
		perror("opendir");
		exit(1);
	}

	while ((dir_entry = readdir(dir))) {
		dir_name = dir_entry->d_name;

		if (dir_name[0] == '.')
			continue;

		if (is_directory(dir_name) == 1) {
			fprintf(stderr, "D: %s ", dir_name);
			if (strstr(dir_name, "send")) {
				fputs("it is send log\n", stderr);
			} else if (strstr(dir_name, "smtp")) {
				fputs("it is smtp log\n", stderr);
			} else
				fputs("I don't know\n", stderr);
		}
	}

	closedir(dir);
}

int
main(int argc, const char * argv[]) {
	const char * argv0;
	const char * path;
	int timeout = 1;

	path = DEFAULT_PATH;
	argv0 = *argv; argv++; argc--;

	if (argc > 0) {
		timeout = atoi(*argv);
		argv++; argc--;
	} else
		usage(argv0);

	if (argc > 0) {
		path = *argv;
		argv++; argc--;
	}

	if (chdir(path) == -1) {
		fprintf(stderr, "Cannot change directory to '%s': %s\n", path, strerror(errno));
		exit(1);
	}

	detect_log_dirs();

	return 0;
}
