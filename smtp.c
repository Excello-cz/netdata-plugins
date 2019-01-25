/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "callbacks.h"
#include "netdata.h"

#include "smtp.h"

struct statistics {
	int tcp_ok;
	int tcp_deny;
	int tcp_status;
	int tcp_status_sum;
	int tcp_status_count;

	int tcp_end_status_0;
	int tcp_end_status_256;
	int tcp_end_status_25600;
	int tcp_end_status_others;
};

static
void *
smtp_data_init() {
	struct statistics * ret;
	ret = calloc(1, sizeof * ret);
	return ret;
}

static
void
process_smtp(const char * line, struct statistics * data) {
	char * ptr;
	int val;

	if (strstr(line, "tcpserver: ok")) {
		data->tcp_ok++;
	}
	if (strstr(line, "tcpserver: deny")) {
		data->tcp_deny++;
	}
	if ((ptr = strstr(line, "tcpserver: status: "))) {
		val = strtoul(ptr + sizeof "tcpserver: status: " - 1, 0, 0);
		data->tcp_status_sum += val;
		data->tcp_status_count++;
		//fprintf(stderr, "v: %d s: %d\n", val, data->tcp_status_sum);
	}
	if ((ptr = strstr(line, "tcpserver: end "))) {
		ptr = strstr(ptr, "status ");
		if (ptr) {
			val = strtoul(ptr + sizeof "status " - 1, 0, 0);
			switch (val) {
			case 0:
				data->tcp_end_status_0++;
				break;
			case 256:
				data->tcp_end_status_256++;
				break;
			case 25600:
				data->tcp_end_status_25600++;
				break;
			default:
				data->tcp_end_status_others++;
				break;
			}
		}
	}
}

static
void
print_smtp_header(const char * name) {
	nd_chart("qmail", name, "smtpd", "smtpd qmail", "Qmail SMTPD", "# smtpd connections",
		"smtpd", "con", ND_CHART_TYPE_AREA);
	nd_dimension("tcp_ok",   "TCP OK",   ND_ALG_ABSOLUTE,  1, 1, ND_VISIBLE);
	nd_dimension("tcp_deny", "TCP Deny", ND_ALG_ABSOLUTE, -1, 1, ND_VISIBLE);

	nd_chart("qmail", name, "smtpd_status", "smtpd statuses", "Qmail SMTPD Statuses",
		"average status", "smtpd", NULL, ND_CHART_TYPE_LINE);
	nd_dimension("tcp_status_average", "status average", ND_ALG_ABSOLUTE, 1, 100, ND_VISIBLE);

	nd_chart("qmail", name, "smtpd_end_status", "smtpd end statuses", "Qmail SMTPD End Statuses",
		"# smtpd end statuses", "smtpd", NULL, ND_CHART_TYPE_LINE);
	nd_dimension("tcp_end_status_0",      "0",     ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("tcp_end_status_256",    "256",   ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("tcp_end_status_25600",  "25600", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("tcp_end_status_others", "other", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	fflush(stdout);
}

static
void
print_smtp_data(const char * name, const struct statistics * data, const unsigned long time) {
	nd_begin_time("qmail", name, "smtpd", time);
	nd_set("tcp_ok", data->tcp_ok);
	nd_set("tcp_deny", data->tcp_deny);
	nd_end();

	nd_begin_time("qmail", name, "smtpd_status", time);
	nd_set("tcp_status_average", data->tcp_status);
	nd_end();

	nd_begin_time("qmail", name, "smtpd_end_status", time);
	nd_set("tcp_end_status_0", data->tcp_end_status_0);
	nd_set("tcp_end_status_256", data->tcp_end_status_256);
	nd_set("tcp_end_status_25600", data->tcp_end_status_25600);
	nd_set("tcp_end_status_others", data->tcp_end_status_others);
	nd_end();

	fflush(stdout);
}

static
void
clear_smtp_data(struct statistics * data) {
	int tmp = data->tcp_status;
	memset(data, 0, sizeof * data);
	data->tcp_status = tmp;
}

static
void
postprocess_data(struct statistics * data) {
	if (data->tcp_status_count)
		data->tcp_status = data->tcp_status_sum * 100 / data->tcp_status_count;
}

static
struct stat_func smtp = {
	.init = &smtp_data_init,
	.fini = &free,
	.print_hdr = &print_smtp_header,
	.print = &print_smtp_data,
	.process = &process_smtp,
	.postprocess = &postprocess_data,
	.clear = &clear_smtp_data,
};

struct stat_func * smtp_func = &smtp;
