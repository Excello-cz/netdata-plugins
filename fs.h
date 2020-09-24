/* SPDX-License-Identifier: GPL-3.0-or-later */

enum watch_type {
	WATCH_LOG_FILE,
	WATCH_QUEUE,
	WATCH_DUMMY,
};

enum skip {
	DO_NOT_SKIP = 0,
	SKIP_THE_REST
};

struct fs_watch {
	const char * dir_name;
	const char * file_name;
	int watch_dir;
	int fd;
	char buf[BUFSIZ];
	ssize_t buffered;
	enum skip skip;
	struct timespec time;
	void * data;
	const struct stat_func * func;
	enum watch_type type;
};

int is_directory(const char *);

enum nd_err read_log_file(struct fs_watch *);
int prepare_fs_event_fd();
void process_fs_event_queue(const int, struct fs_watch *, size_t);
