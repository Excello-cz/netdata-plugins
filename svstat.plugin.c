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
#include "vector.h"

#include "fs.h"

#define DEFAULT_PATH "/service"

struct dt_stat {
	uint64_t seconds;
	uint32_t nano;
	uint32_t pid;
	uint8_t  paused;
	uint8_t  want;
};

enum status {
	SUCCESS,
	ERR_CHDIR,
	ERR_OPEN,
	ERR_READ,
};

struct statistics {
	struct {
		uint64_t timestamp;
		int is_up;
		enum status err;
	} data;
	const char * name;
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

void
collect_uptime(struct statistics * statistics) {
	const char * dir = statistics->name;
	unsigned char status[18]; /* See daemontools code */
	struct dt_stat * stat;
	int fd, ret;

	if (chdir(dir) == -1) {
		fprintf(stderr, "Cannot change directory to '%s': %s\n", dir, strerror(errno));
		statistics->data.err = ERR_CHDIR;
		return;
	}

	fd = open("supervise/status", O_RDONLY | O_NDELAY);
	if (fd == -1) {
		fprintf(stderr, "Cannot open %s/supervise/status: %s\n", dir, strerror(errno));
		statistics->data.err = ERR_OPEN;
		return;
	}

	ret = read(fd, status, sizeof status);
	close(fd);

	if (ret < sizeof status) {
		fprintf(stderr, "Cannot read supervise/status\n");
		statistics->data.err = ERR_READ;
		return;
	}

	stat = (void *)status;
	statistics->data.timestamp = be64toh(stat->seconds);
	statistics->data.is_up = !!stat->pid;
	statistics->data.err = SUCCESS;
}

int
main(int argc, char * argv[]) {
	struct vector directories = VECTOR_EMPTY;
	struct statistics statistics;
	unsigned long last_update;
	struct timespec timestamp;
	struct dirent * dir_entry;
	const char * dir_name;
	const char * argv0;
	const char * path;
	int timeout = 1;
	int dir_fd;
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

	vector_init(&directories, sizeof statistics);
	memset(&statistics, 0, sizeof statistics);

	dir = opendir(".");
	while ((dir_entry = readdir(dir))) {
		dir_name = dir_entry->d_name;

		if (dir_name[0] == '.')
			continue;

		if (is_directory(dir_name) == 1) {
			statistics.name = strdup(dir_name);
			if (statistics.name) {
				vector_add(&directories, &statistics);
			}
		}
	}
	closedir(dir);

	if (vector_is_empty(&directories)) {
		fprintf(stderr, "No service directory detected\n");
		exit(1);
	}

	dir_fd = open(".", O_DIRECTORY | O_RDONLY | O_NDELAY);
	if (dir_fd == -1) {
		fprintf(stderr, "Cannot open directory '%s': %s\n", path, strerror(errno));
		exit(1);
	}

	nd_chart("daemontools", "uptime", NULL, NULL, "Service Uptime", "seconds", "daemontools", "daemontools.uptime", ND_CHART_TYPE_LINE);
	for (int i = 0; i < directories.len; i++) {
		struct statistics * st = vector_item(&directories, i);
		nd_dimension(st->name, st->name, ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	}

	nd_chart("daemontools", "downtime", NULL, NULL, "Service Downtime", "seconds", "daemontools", "daemontools.downtime", ND_CHART_TYPE_LINE);
	for (int i = 0; i < directories.len; i++) {
		struct statistics * st = vector_item(&directories, i);
		nd_dimension(st->name, st->name, ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	}

	nd_chart("daemontools", "up_down", NULL, NULL, "Service Up/Down", "up/down", "daemontools", "daemontools.up_down", ND_CHART_TYPE_LINE);
	for (int i = 0; i < directories.len; i++) {
		struct statistics * st = vector_item(&directories, i);
		nd_dimension(st->name, st->name, ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	}
	fflush(stdout);

	clock_gettime(CLOCK_REALTIME, &timestamp);

	for (run = 1; run;) {
		/* Collect statistics */
		for (int i = 0; i < directories.len; i++) {
			struct statistics * st = vector_item(&directories, i);
			memset(&st->data, 0, sizeof st->data);
			collect_uptime(st);
			if (fchdir(dir_fd) == -1) {
				fprintf(stderr, "Cannot change directory back to '%s': %s\n", path, strerror(errno));
				break;
			}
		}

		/* Present statistics */
		last_update = update_timestamp(&timestamp);
		time_t now = tai_now();

		nd_begin_time("daemontools", "uptime", NULL, last_update);
		for (int i = 0; i < directories.len; i++) {
			struct statistics * st = vector_item(&directories, i);
			if (st->data.err == SUCCESS && st->data.is_up) {
				nd_set(st->name, now - st->data.timestamp);
			}
		}
		nd_end();

		nd_begin_time("daemontools", "downtime", NULL, last_update);
		for (int i = 0; i < directories.len; i++) {
			struct statistics * st = vector_item(&directories, i);
			if (st->data.err == SUCCESS) {
				nd_set(st->name, !st->data.is_up ? now - st->data.timestamp : 0);
			}
		}
		nd_end();

		nd_begin_time("daemontools", "up_down", NULL, last_update);
		for (int i = 0; i < directories.len; i++) {
			struct statistics * st = vector_item(&directories, i);
			if (st->data.err == SUCCESS) {
				nd_set(st->name, st->data.is_up);
			}
		}
		nd_end();

		fflush(stdout);

		sleep(timeout);
	}
	close(dir_fd);
	for (int i = 0; i < directories.len; i++) {
		free((void *)((struct statistics *)vector_item(&directories, i))->name);
	}
	vector_free(&directories);
	return 0;
}
