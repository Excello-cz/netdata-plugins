/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "callbacks.h"
#include "fs.h"

int
is_directory(const char * name) {
	struct stat st;
	int ret;

	ret = stat(name, &st);

	if (ret == -1)
		return ret;

	return S_ISDIR(st.st_mode);
}

int
prepare_fs_event_fd() {
	int fd;

	fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

	if (fd == -1) {
		perror("inotify_init1");
		exit(1);
	}

	return fd;
}

void
read_log_file(struct fs_event * watch) {
	char buf[BUFSIZ];
	ssize_t ret;
	char * pos;
	char * end;

	while ((ret = read(watch->fd, buf, sizeof buf)) > 0) {
		//fprintf(stderr, "D: read %ld from %s\n", ret, watch->dir_name);
		pos = buf;
		for (;;) {
			end = memchr(pos, '\n', ret - (pos - buf));
			if (end) {
				*end = '\0';
				//fprintf(stderr, "D: line %ld %ld\n", strlen(pos), end - pos);
				watch->func->process(pos, watch->data);
				pos = end + 1;
			} else {
				//fputs("Could not locate eol\n", stderr);
				break;
			}
		}
	}
}

static
void
reopen_log_file(struct fs_event * watch) {
	char file_name[BUFSIZ];

	fprintf(stderr, "D: reopening file log file in: %s\n", watch->dir_name);
	sprintf(file_name, "%s/" CURRENT_LOG_FILE_NAME, watch->dir_name);
	close(watch->fd);
	watch->fd = open(file_name, O_RDONLY);
}

static
void
process_fs_event(const struct inotify_event * event, struct fs_event * logs, size_t len) {
	int i;

	for (i = 0; i < len; i++) {
		struct fs_event * item = logs + i;
		if (event->wd == item->watch_file) {
			fprintf(stderr, "%s/current changed\n", item->dir_name);
		} else if (event->wd == item->watch_dir) {
			fprintf(stderr, "%s directory changed\n", item->dir_name);
			if (event->len) {
				fprintf(stderr, "the name of the change: %s\n", event->name);
				if (!strcmp(event->name, CURRENT_LOG_FILE_NAME)) {
					read_log_file(item);
					reopen_log_file(item);
				}
			}
		}
	}
}

void
process_fs_event_queue(const int fd, struct fs_event * logs, size_t logs_len) {
	const struct inotify_event * event;
	char buf[BUFSIZ];
	ssize_t len;
	char * ptr;

	for (;;) {
		len = read(fd, buf, sizeof buf);
		if (len == -1 && errno != EAGAIN) {
			perror("E: Cannot read fs_event fd");
			exit(1);
		}

		if (len <= 0)
			break;

		for (ptr = buf; ptr < buf + len; ptr += sizeof * event + event->len) {
			event = (const struct inotify_event *)ptr;
			process_fs_event(event, logs, logs_len);
		}
	}
}
