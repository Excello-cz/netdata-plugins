/* SPDX-License-Identifier: GPL-3.0-or-later */

struct fs_watch {
	const char * dir_name;
	const char * file_name;
	int watch_dir;
	int fd;
	struct timespec time;
	void * data;
	const struct stat_func * func;
};

int is_directory(const char *);

enum nd_err read_log_file(struct fs_watch *);
int prepare_fs_event_fd();
void process_fs_event_queue(const int, struct fs_watch *, size_t);
