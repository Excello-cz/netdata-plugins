#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>

#include "err.h"

#define LOG_DIR  0
#define LOG_FILE 1

#define DEFALT_PATH "/var/log/qmail/qmail-smtpd/current"

static int log_fd; /* smtp log file descriptor */
static int ino_fd; /* inotify file descriptor */
static int wd[2];  /* Watch descriptors for inotify */

static
enum nd_err
set_wd_for_log_directory(const int fd, const char * file_name) {
	char * file_name_copy;

	file_name_copy = strdup(file_name);
	if (file_name_copy == NULL)
		return ND_ALLOC;

	wd[LOG_DIR] = inotify_add_watch(fd, dirname(file_name_copy), IN_CREATE);

	if (wd[LOG_DIR] == -1)
		return ND_INOTIFY;

	free(file_name_copy);

	return ND_SUCCUESS;
}

static
enum nd_err
set_wd_for_log_file(const int fd, const char * file_name) {
	wd[LOG_FILE] = inotify_add_watch(fd, file_name, IN_MODIFY);

	if (wd[LOG_FILE] == -1)
		return ND_INOTIFY;

	return ND_SUCCUESS;
}

int
main(int argc, char * argv[]) {
	const char * file_name = DEFALT_PATH;
	const char * argv0;
	enum nd_err ret;
	int update = 1;

	argv0 = *argv; argv++; argc--;

	if (argc >= 1) {
		update = atoi(*argv);
		argv++; argc--;
	} else {
		fprintf(stderr, "Usage: %s <update> [path]\n", argv0);
	}

	if (argc) {
		file_name = *argv;
		argv++; argc--;
	}

	ino_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
	if (ino_fd == -1) {
		fprintf(stderr, "Cannot init inotify\n");
		exit(1);
	}

	ret = set_wd_for_log_directory(ino_fd, file_name);
	if (ret != ND_SUCCUESS) {
		fprintf(stderr, "Cannot watch log dir: %s\n", nd_err_to_str(ret));
		exit(1);
	}
	ret = set_wd_for_log_file(ino_fd, file_name);
	if (ret != ND_SUCCUESS) {
		fprintf(stderr, "Cannot watch log file: %s\n", nd_err_to_str(ret));
		exit(1);
	}

	return 0;
}
