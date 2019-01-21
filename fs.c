#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <sys/stat.h>

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
