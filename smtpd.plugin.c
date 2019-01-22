#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "callbacks.h"
#include "err.h"
#include "vector.h"
#include "flush.h"
#include "fs.h"
#include "netdata.h"
#include "smtp.h"
#include "signal.h"
#include "timer.h"

#define LOG_DIR  0
#define LOG_FILE 1

#define POLL_INOTIFY 0
#define POLL_TIMER   1
#define POLL_SIGNAL  2

#define DEFALT_PATH "/var/log/qmail/qmail-smtpd/current"

#define LEN(x) ( sizeof x / sizeof * x )

static int log_fd; /* smtp log file descriptor */
static int ino_fd; /* inotify file descriptor */
static int wd[2];  /* Watch descriptors for inotify */

enum event_result {
	ND_NOTHING,
	ND_REOPEN_LOG_FILE,
};

static
enum nd_err
set_wd_for_log_directory(const int fd, const char * file_name) {
	char * file_name_copy;

	file_name_copy = strdup(file_name);
	if (file_name_copy == NULL)
		return ND_ALLOC;

	wd[LOG_DIR] = inotify_add_watch(fd, dirname(file_name_copy), IN_CREATE);

	if (wd[LOG_DIR] == -1)
		return ND_INOTIFY;

	free(file_name_copy);

	return ND_SUCCESS;
}

static
enum nd_err
set_wd_for_log_file(const int fd, const char * file_name) {
	wd[LOG_FILE] = inotify_add_watch(fd, file_name, IN_MODIFY);

	if (wd[LOG_FILE] == -1)
		return ND_INOTIFY;

	return ND_SUCCESS;
}

static
enum nd_err
reopen_log_file(const char * file_name) {
	inotify_rm_watch(ino_fd, wd[LOG_FILE]);
	close(log_fd);

	wd[LOG_FILE] = inotify_add_watch(ino_fd, file_name, IN_MODIFY);

	if (wd[LOG_FILE] == -1)
		return ND_INOTIFY;

	//fprintf(stderr, "D: reopening file\n");
	log_fd = open(file_name, O_RDONLY);
	if (log_fd == -1)
		return ND_FILE;

	return ND_SUCCESS;
}

static
enum nd_err
init_inotifier(const char * file_name) {
	enum nd_err ret;

	ino_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
	if (ino_fd == -1) {
		fprintf(stderr, "E: Cannot init inotify\n");
		return ND_INOTIFY;
	}

	ret = set_wd_for_log_directory(ino_fd, file_name);
	if (ret != ND_SUCCESS) {
		fprintf(stderr, "E: Cannot watch log dir: %s\n", nd_err_to_str(ret));
		return ret;
	}

	ret = set_wd_for_log_file(ino_fd, file_name);
	if (ret != ND_SUCCESS) {
		fprintf(stderr, "E: Cannot watch log file: %s\n", nd_err_to_str(ret));
		return ret;
	}

	return ND_SUCCESS;
}

static
enum event_result
handle_inot_event(const struct inotify_event * event) {
	if (event->wd == wd[LOG_DIR]) {
		//fprintf(stderr, "D: logdir event\n");
		if (event->len && !strcmp(event->name, "current")) {
			//fprintf(stderr, "D: there is new 'current'\n");
			return ND_REOPEN_LOG_FILE;
		}
	} else if (event->wd == wd[LOG_FILE]) {
		//fprintf(stderr, "D: logfile evnet\n");
	}

	return ND_NOTHING;
}

static
enum event_result
handle_inot_events() {
	enum event_result ret = ND_NOTHING;
	const struct inotify_event * event;
	char buf[BUFSIZ];
	ssize_t len;
	char * ptr;

	for (;;) {
		len = read(ino_fd, buf, sizeof buf);
		if (len == -1 && errno != EAGAIN) {
			perror("E: cannot read inofd");
			exit(1);
		}

		if (len <= 0)
			break;

		for (ptr = buf; ptr < buf + len; ptr += sizeof * event + event->len) {
			event = (const struct inotify_event *)ptr;
			if (handle_inot_event(event) == ND_REOPEN_LOG_FILE)
				ret = ND_REOPEN_LOG_FILE;
		}
	}

	return ret;
}

static
void
process_log_data(const int fd, struct statistics * data) {
	char buf[BUFSIZ];
	ssize_t ret;

	while ((ret = read(fd, buf, sizeof buf - 1)) > 0) {
		buf[ret] = '\0';
		smtp_func->process(buf, data);
	}
}

int
main(int argc, char * argv[]) {
	const char * file_name = DEFALT_PATH;
	enum event_result event_result;
	struct statistics data;
	struct pollfd pfd[3];
	const char * argv0;
	enum nd_err ret;
	int update = 1;
	int signal_fd;
	int timer_fd;
	int nfd;
	int run;

	argv0 = *argv; argv++; argc--;

	if (argc > 0) {
		update = atoi(*argv);
		fprintf(stderr, "smtpd: D: setting update period to: %d s\n", update);
		argv++; argc--;
	} else {
		fprintf(stderr, "Usage: %s <update> [path]\n", argv0);
	}

	if (argc > 0) {
		file_name = *argv;
		fprintf(stderr, "smtpd: D: setting watched log file to: %s\n", file_name);
		argv++; argc--;
	}

	signal_fd = prepare_signal_fd();
	pfd[POLL_SIGNAL].fd = signal_fd;
	pfd[POLL_SIGNAL].events = POLLIN;

	ret = init_inotifier(file_name);
	if (ret != ND_SUCCESS) {
		fprintf(stderr, "smtpd: E: cannot establish inotifier: %s\n", nd_err_to_str(ret));
		exit(1);
	}
	log_fd = open(file_name, O_RDONLY);
	if (log_fd == -1) {
		perror("smtpd: E: Cannot open file");
		exit(1);
	}
	lseek(log_fd, 0, SEEK_END);

	pfd[POLL_INOTIFY].fd = ino_fd;
	pfd[POLL_INOTIFY].events = POLLIN;

	timer_fd = prepare_timer_fd(update);
	pfd[POLL_TIMER].fd = timer_fd;
	pfd[POLL_TIMER].events = POLLIN;

	memset(&data, 0, sizeof data);
	fputs("smtpd: D: Starting smptd.plugin\n", stderr);
	smtp_func->print_hdr();

	for (run = 1; run ;) {
		nfd = poll(pfd, LEN(pfd), -1);
		if (nfd > 0) {
			if (pfd[POLL_SIGNAL].revents & POLLIN) {
				flush_read_fd(signal_fd);
				run = 0;
				continue;
			}
			if (pfd[POLL_INOTIFY].revents & POLLIN) {
				event_result = handle_inot_events();
				process_log_data(log_fd, &data);
				if (event_result == ND_REOPEN_LOG_FILE) {
					reopen_log_file(file_name);
				}
			}
			if (pfd[POLL_TIMER].revents & POLLIN) {
				flush_read_fd(pfd[POLL_TIMER].fd);
				smtp_func->postprocess(&data);
				smtp_func->print(&data);
				smtp_func->clear(&data);
			}
		} else if (nfd == 0) {
			fputs("smtpd: D: timeout\n", stderr);
			continue;
		} else {
			if (errno == EINTR)
				continue;
			perror("smtpd: E: poll error");
			break;
		}
	}

	fputs("smtpd: D: Exiting\n", stderr);

	close(ino_fd);
	close(log_fd);
	close(timer_fd);
	close(signal_fd);

	return ret;
}
