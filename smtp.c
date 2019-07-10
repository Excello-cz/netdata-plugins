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

	int smtp;
	int esmtps;

	int esmtps_tls_1;
	int esmtps_tls_1_1;
	int esmtps_tls_1_2;
	int esmtps_tls_1_3;
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
	} else if (strstr(line, "tcpserver: deny")) {
		data->tcp_deny++;
	} else if ((ptr = strstr(line, "tcpserver: status: "))) {
		val = strtoul(ptr + sizeof "tcpserver: status: " - 1, 0, 0);
		data->tcp_status_sum += val;
		data->tcp_status_count++;
	} else if ((ptr = strstr(line, "tcpserver: end "))) {
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
	} else if ((ptr = strstr(line, "uses ESMTPS"))) {
		data->esmtps++;
		if (strstr(ptr, "TLSv1,")) {
			data->esmtps_tls_1++;
		} else if (strstr(ptr, "TLSv1.1,")) {
			data->esmtps_tls_1_1++;
		} else if (strstr(ptr, "TLSv1.2,")) {
			data->esmtps_tls_1_2++;
		} else if (strstr(ptr, "TLSv1.3,")) {
			data->esmtps_tls_1_3++;
		}
	} else if ((ptr = strstr(line, "uses SMTP"))) {
		data->smtp++;
	}
}

static
void
print_smtp_header(const char * name) {
	char title[BUFSIZ];

	sprintf(title, "Qmail SMTPD for %s", name);
	nd_chart("qmail", name, "", "smtpd qmail", title, "# smtpd connections",
		"smtpd", "con", ND_CHART_TYPE_AREA);
	nd_dimension("tcp_ok",   "TCP OK",   ND_ALG_ABSOLUTE,  1, 1, ND_VISIBLE);
	nd_dimension("tcp_deny", "TCP Deny", ND_ALG_ABSOLUTE, -1, 1, ND_VISIBLE);

	sprintf(title, "Qmail SMTPD Open Sessions for %s", name);
	nd_chart("qmail", name, "status", "smtpd statuses", title,
		"average status", "smtpd", NULL, ND_CHART_TYPE_LINE);
	nd_dimension("tcp_status_average", "session average", ND_ALG_ABSOLUTE, 1, 100, ND_VISIBLE);

	sprintf(title, "Qmail SMTPD End Statuses for %s", name);
	nd_chart("qmail", name, "end_status", "smtpd end statuses", title,
		"# smtpd end statuses", "smtpd", NULL, ND_CHART_TYPE_LINE);
	nd_dimension("tcp_end_status_0",      "0",     ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("tcp_end_status_256",    "256",   ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("tcp_end_status_25600",  "25600", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("tcp_end_status_others", "other", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	sprintf(title, "Qmail SMTPD smtp type for %s", name);
	nd_chart("qmail", name, "smtp_type", "smtp type", title, "# smtp protocol",
		"smtpd", "", ND_CHART_TYPE_LINE);
	nd_dimension("smtp",	 "SMTP",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("esmtps", "ESMTPS", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	sprintf(title, "Qmail SMTPD tls connection types for %s", name);
	nd_chart("qmail", name, "tls", "tls version", title, "# tls version",
		"smtpd", "", ND_CHART_TYPE_LINE);
	nd_dimension("tls1",	 "TLS_1",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("tls1.1",	 "TLS_1.1",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("tls1.2",	 "TLS_1.2",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("tls1.3",	 "TLS_1.3",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);

	fflush(stdout);
}

static
void
print_smtp_data(const char * name, const struct statistics * data, const unsigned long time) {
	nd_begin_time("qmail", name, "", time);
	nd_set("tcp_ok", data->tcp_ok);
	nd_set("tcp_deny", data->tcp_deny);
	nd_end();

	nd_begin_time("qmail", name, "status", time);
	nd_set("tcp_status_average", data->tcp_status);
	nd_end();

	nd_begin_time("qmail", name, "end_status", time);
	nd_set("tcp_end_status_0", data->tcp_end_status_0);
	nd_set("tcp_end_status_256", data->tcp_end_status_256);
	nd_set("tcp_end_status_25600", data->tcp_end_status_25600);
	nd_set("tcp_end_status_others", data->tcp_end_status_others);
	nd_end();

	nd_begin_time("qmail", name, "smtp_type", time);
	nd_set("smtp", data->smtp);
	nd_set("esmtps", data->esmtps);
	nd_end();

	nd_begin_time("qmail", name, "tls", time);
	nd_set("tls1", data->esmtps_tls_1);
	nd_set("tls1.1", data->esmtps_tls_1_1);
	nd_set("tls1.2", data->esmtps_tls_1_2);
	nd_set("tls1.3", data->esmtps_tls_1_3);
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
