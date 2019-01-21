#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "flush.h"
#include "fs.h"
#include "signal.h"
#include "timer.h"

#define DEFAULT_PATH "/var/log/qmail"

enum poll {
	POLL_SIGNAL = 0,
	POLL_TIMER,
	POLL_LENGTH
};

#define LEN(x) ( sizeof x / sizeof * x )

struct processor {
	void (*clear)        (void *);
	void (*print)        (const void *);
	void (*process)      (const char *, void *);
	void (*postprocess)  (void *);
};

struct fs_event {
	const char * dir_name;
	/* NOTE: There is no need to save file name, it is 'current' always */
	int watch_dir;
	int watch_file;
	int file_fd;
	void * data;
	struct processor * proc;
};

static
void
usage(const char * name) {
	fprintf(stderr, "usage: %s <timout> [path]\n", name);
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
				/* TODO: register send notifier for this directory */
			} else if (strstr(dir_name, "smtp")) {
				fputs("it is smtp log\n", stderr);
				/* TODO: register smtp notifier for this directory */
			} else
				fputs("I don't know\n", stderr);
		}
	}

	closedir(dir);
}

int
main(int argc, const char * argv[]) {
	struct pollfd pfd[POLL_LENGTH];
	const char * argv0;
	const char * path;
	int timeout = 1;
	int signal_fd;
	int timer_fd;
	int run;

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

	if (is_directory(path) < 1) {
		fprintf(stderr, "Cannot change dir to %s, it is not a directory\n", path);
		/* TODO: Maybe we can distinguish all return values
		 *  1 - it is a directory
		 *  0 - it is not a directory
		 * -1 - an error; file does not exit, etc.
		 */
		exit(1);
	}
	if (chdir(path) == -1) {
		fprintf(stderr, "Cannot change directory to '%s': %s\n", path, strerror(errno));
		exit(1);
	}

	detect_log_dirs();

	timer_fd = prepare_timer_fd(timeout);
	pfd[POLL_TIMER].fd = timer_fd;
	pfd[POLL_TIMER].events = POLLIN;

	signal_fd = prepare_signal_fd();
	pfd[POLL_SIGNAL].fd = signal_fd;
	pfd[POLL_SIGNAL].events = POLLIN;

	for (run = 1; run;) {
		switch (poll(pfd, LEN(pfd), -1)) {
		case -1:
			perror("poll");
			break;
		case 0:
			fputs("timeout\n", stderr);
			continue;
		default:
			if (pfd[POLL_SIGNAL].revents & POLLIN) {
				flush_read_fd(signal_fd);
				run = 0;
				continue;
			}
			/* TODO: dir event notifier:
			 *
			 * - it detects new directories in DEFAULT_PATH
			 * - it detects 'current' log rotations and reopes and reregister it again
			 * - it detects change in every registerd 'current' log and process appended lines
			 */
			if (pfd[POLL_TIMER].revents & POLLIN) {
				fprintf(stderr, "time to print\n");
				flush_read_fd(timer_fd);
				/* TODO: postprocess collected data */
				/* TODO: print out all statistics for all registered directories */
			}
		}
	}

	close(timer_fd);
	close(signal_fd);

	return 0;
}
