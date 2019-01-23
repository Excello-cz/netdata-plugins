/* SPDX-License-Identifier: GPL-3.0-or-later */

#define CURRENT_LOG_FILE_NAME "current"

struct fs_event {
	const char * dir_name;
	/* NOTE: There is no need to save file name, it is 'current' always */
	int watch_dir;
	int watch_file;
	int fd;
	struct timespec time;
	long last_update;
	void * data;
	const struct stat_func * func;
};

int is_directory(const char *);

void update_timestamps(struct fs_event *);

void read_log_file(struct fs_event *);
int prepare_fs_event_fd();
void process_fs_event_queue(const int, struct fs_event *, size_t);
