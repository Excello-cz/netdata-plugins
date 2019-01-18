#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "flush.h"

int
flush_read_fd(const int fd) {
	char buf[BUFSIZ];
	ssize_t ret;

	while ((ret = read(fd, buf, sizeof buf)) > 0)
		;

	if (ret == -1 && errno != EAGAIN)
		return -1;

	return 0;
}
