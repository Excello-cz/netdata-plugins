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
	ssize_t  max_line_length;
	const char * line;
	char buf[BUFSIZ];
	ssize_t buffered;
	ssize_t ret;
	char * end;

	enum skip {
		DO_NOT_SKIP,
		SKIP_THE_REST
	} skip;

	buffered = 0;
	skip = DO_NOT_SKIP;

	while ((ret = read(watch->fd, buf + buffered, sizeof buf - buffered)) > 0) {
		line = buf;
		ret += buffered;
		buffered = 0;
		for (;;) {
			max_line_length = ret - (line - buf);
			end = memchr(line, '\n', max_line_length);
			if (end) {
				*end = '\0';

				if (skip == DO_NOT_SKIP)
					watch->func->process(line, watch->data);
				else
					skip = DO_NOT_SKIP;

				line = end + 1;
			} else {
				if (max_line_length > 0 && max_line_length < BUFSIZ) {
					buffered = max_line_length;
					memmove(buf, line, buffered);
				} else if (max_line_length == BUFSIZ) {
					buf[BUFSIZ - 1] = '\0';

					if (skip == DO_NOT_SKIP)
						watch->func->process(line, watch->data);

					skip = SKIP_THE_REST;
				}
				break;
			}
		}
	}
}

static
void
reopen_log_file(struct fs_event * watch) {
	char file_name[BUFSIZ];

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
		if (event->wd == item->watch_dir) {
			if (event->len) {
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
