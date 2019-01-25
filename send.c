/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "callbacks.h"
#include "netdata.h"
#include "send.h"

struct send_statistics {
	int start_delivery;
	int end_msg;

	int delivery_success;
	int delivery_failure;
	int delivery_deferral;
};

static
void *
send_data_init() {
	struct send_statistics * ret;

	ret = calloc(1, sizeof * ret);
	return ret;
}

static
void
clear_send_statistics(struct send_statistics * data) {
	memset(data, 0, sizeof * data);
}

static
void
print_send_hdr(const char * name) {
	char title[BUFSIZ];

	sprintf(title, "Qmail Send for %s", name);
	nd_chart("qmail", name, "", "send qmail", title, "# send", NULL, "send", ND_CHART_TYPE_AREA);
	nd_dimension("start_delivery", "Start/End Delivery", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("end_msg", "End Msg", ND_ALG_ABSOLUTE, -1, 1, ND_VISIBLE);

	sprintf(title, "Qmail Send delivery status for %s", name);
	nd_chart("qmail", name, "delivery", "send delivery", title,
		"# deliveries", NULL, "send_delivery", ND_CHART_TYPE_LINE);
	nd_dimension("delivery_success",  "Success", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("delivery_failure",  "Failure", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("delivery_deferral", "Deferral", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	fflush(stdout);
}

static
void
print_send_data(const char * name, const struct send_statistics * data, const unsigned long time) {
	nd_begin_time("qmail", name, "", time);
	nd_set("start_delivery", data->start_delivery);
	nd_set("end_msg", data->end_msg);
	nd_end();

	nd_begin_time("qmail", name, "delivery", time);
	nd_set("delivery_success", data->delivery_success);
	nd_set("delivery_failure", data->delivery_failure);
	nd_set("delivery_deferral", data->delivery_deferral);
	nd_end();

	fflush(stdout);
}

static
void
process_send_log_line(const char * line, struct send_statistics * data) {
	const char * ptr;

	if (strstr(line, "starting delivery")) {
		data->start_delivery++;
	} else if (strstr(line, "end msg")) {
		data->end_msg++;
	} else if ((ptr = strstr(line, "delivery "))) {
		if (strstr(ptr, "success:")) {
			data->delivery_success++;
		} else if (strstr(ptr, "failure:")) {
			data->delivery_failure++;
		} else if (strstr(ptr, "deferral:")) {
			data->delivery_deferral++;
		}
	}
}

static
struct stat_func send = {
	.init = &send_data_init,
	.fini = &free,

	.print_hdr = &print_send_hdr,
	.print = &print_send_data,
	.process = &process_send_log_line,
	.postprocess = NULL,
	.clear = &clear_send_statistics,
};

struct stat_func * send_func = &send;
