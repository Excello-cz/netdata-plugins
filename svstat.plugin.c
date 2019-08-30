/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "err.h"
#include "netdata.h"
#include "timer.h"

#include "fs.h"

#define DEFAULT_PATH "/service"

struct dt_stat {
	uint64_t seconds;
	uint32_t nano;
	uint32_t pid;
	uint8_t  paused;
	uint8_t  want;
};

int run;

static
void
usage(const char * name) {
	fprintf(stderr, "usage: %s <timeout> [path]\n", name);
}

static inline
uint64_t
tai_now() {
	return 4611686018427387914ULL + time(NULL);
}

static
void
quit(int unused) {
	run = 0;
}

uint64_t
collect_uptime(const char * dir) {
	unsigned char status[18]; /* See daemontools code */
	struct dt_stat * stat;
	uint64_t time;
	int fd, ret;

	if (chdir(dir) == -1) {
		fprintf(stderr, "Cannot change directory to '%s': %s\n", dir, strerror(errno));
		return 0;
	}

	fd = open("supervise/status", O_RDONLY | O_NDELAY);
	if (fd == -1) {
		fprintf(stderr, "Cannot open supervise/status: %s\n", strerror(errno));
		return 0;
	}

	ret = read(fd, status, sizeof status);
	if (ret < sizeof status) {
		fprintf(stderr, "Cannot read supervise/status\n");
		return 0;
	}
	close(fd);

	stat = (void *)status;
	time = tai_now() - be64toh(stat->seconds);
	if (!stat->pid)
		time *= -1;

	return time;
}

int
main(int argc, char * argv[]) {
	unsigned long last_update;
	struct timespec timestamp;
	struct dirent * dir_entry;
	const char * dir_name;
	const char * argv0;
	const char * path;
	int timeout = 1;
	DIR * dir;

	path = DEFAULT_PATH;
	argv0 = *argv; argv++; argc--;

	if (argc > 0) {
		timeout = atoi(*argv);
		argv++; argc--;
	} else {
		usage(argv0);
	}

	if (argc > 0) {
		path = *argv;
		argv++; argc--;
	}

	if (chdir(path) == -1) {
		fprintf(stderr, "Cannot change directory to '%s': %s\n", path, strerror(errno));
		exit(1);
	}

	signal(SIGQUIT, quit);
	signal(SIGTERM, quit);
	signal(SIGINT, quit);

	dir = opendir(".");
	nd_chart("daemontools", "svstat", NULL, NULL, NULL, "time", NULL, "daemontools.svstat", ND_CHART_TYPE_LINE);
	while ((dir_entry = readdir(dir))) {
		dir_name = dir_entry->d_name;

		if (dir_name[0] == '.')
			continue;

		if (is_directory(dir_name) == 1)
			nd_dimension(dir_name, dir_name, ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	}
	fflush(stdout);
	clock_gettime(CLOCK_REALTIME, &timestamp);

	for (run = 1; run;) {
		rewinddir(dir);
		last_update = update_timestamp(&timestamp);
		nd_begin_time("daemontools", "svstat", NULL, last_update);
		while ((dir_entry = readdir(dir))) {
			dir_name = dir_entry->d_name;

			if (dir_name[0] == '.')
				continue;

			if (is_directory(dir_name) == 1) {
				nd_set(dir_name, collect_uptime(dir_name));
			}

			if (fchdir(dirfd(dir)) == -1) {
				fprintf(stderr, "Cannot change directory back to '%s': %s\n", path, strerror(errno));
				break;
			}
		}
		nd_end();
		fflush(stdout);
		sleep(timeout);
	}
	closedir(dir);
	return 0;
}
