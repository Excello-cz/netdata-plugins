/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>

#include "timer.h"

int
prepare_timer_fd(const int timeout) {
	struct itimerspec tv;
	int ret, fd;

	fd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC | TFD_NONBLOCK);

	if (fd == -1) {
		perror("E: Cannot create timer");
		exit(1);
	}

	memset(&tv, 0, sizeof tv);
	tv.it_interval.tv_sec = timeout;
	tv.it_value.tv_sec = timeout;

	ret = timerfd_settime(fd, 0, &tv, NULL);

	if (ret == -1) {
		perror("E: Cannot set timer");
		exit(1);
	}

	return fd;
}
