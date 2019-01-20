#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>

#include "flush.h"

#define DEFAULT_PATH "/var/log/qmail"
#define POLL_TIMER   0

#define LEN(x) ( sizeof x / sizeof * x )

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
	struct itimerspec timer_value;
	struct pollfd pfd[1];
	const char * argv0;
	const char * path;
	int timeout = 1;
	int timer_fd;
	int ret;

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
		exit(1);
	}
	if (chdir(path) == -1) {
		fprintf(stderr, "Cannot change directory to '%s': %s\n", path, strerror(errno));
		exit(1);
	}

	detect_log_dirs();

	timer_fd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC | TFD_NONBLOCK);
	pfd[POLL_TIMER].fd = timer_fd;
	pfd[POLL_TIMER].events = POLLIN;

	if (timer_fd == -1) {
		perror("E: Cannot create timer");
		exit(1);
	}

	memset(&timer_value, 0, sizeof timer_value);
	timer_value.it_interval.tv_sec = timeout;
	timer_value.it_value.tv_sec = timeout;
	ret = timerfd_settime(timer_fd, 0, &timer_value, NULL);
	if (ret == -1) {
		perror("E: Cannot set timer");
		exit(1);
	}

	for (;;) {
		switch (poll(pfd, LEN(pfd), -1)) {
		case -1:
			perror("poll");
			break;
		case 0:
			fputs("timeout\n", stderr);
			continue;
		default:
			if (pfd[POLL_TIMER].revents & POLLIN) {
				fprintf(stderr, "time to print\n");
				flush_read_fd(timer_fd);
			}
		}
	}

	close(timer_fd);

	return 0;
}
