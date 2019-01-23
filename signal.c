/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/signalfd.h>

#include "signal.h"

int
prepare_signal_fd() {
	sigset_t mask;
	int ret;
	int fd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGINT);

	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (ret == -1) {
		perror("sigprocmask");
		exit(1);
	}

	fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	if (fd == -1) {
		perror("signalfd");
		exit(1);
	}

	return fd;
}
