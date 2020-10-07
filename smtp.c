/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "callbacks.h"
#include "netdata.h"

#include "smtp.h"

/* Netdata collects integer values only. We have to multiply collected value by
 * this constant and set the DIMENSION divider to the same value if we need
 * fractional values.  */
#define FRACTIONAL_CONVERSION 100

struct smtp_statistics {
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
	int esmtps_unknown;

	int queue_err_conn_timeout;
	int queue_err_comm_failed;
	int queue_err_perm_reject;
	int queue_err_refused;
	int queue_err_unprocess;
	int queue_err_unknown;
};

static
void *
smtp_data_init() {
	struct smtp_statistics * ret;
	ret = calloc(1, sizeof * ret);
	return ret;
}

static
void
process_smtp(const char * line, struct smtp_statistics * data) {
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
		} else {
			data->esmtps_unknown++;
		}
	} else if ((ptr = strstr(line, "uses SMTP"))) {
		data->smtp++;
	} else if ((ptr = strstr(line, "qmail-smtpd: qmail-queue error message: "))) {
		if (strstr(ptr, "451 tcp connection to mail server timed out")) {
			data->queue_err_conn_timeout++;
		} else if (strstr(ptr, "451 unable to process message")) {
			data->queue_err_unprocess++;
		} else if (strstr(ptr, "451 tcp connection to mail server succeeded, but communication failed")) {
			data->queue_err_comm_failed++;
		} else if (strstr(ptr, "554 mail server permanently rejected message")) {
			data->queue_err_perm_reject++;
		} else if (strstr(ptr, "554 message refused")) {
			data->queue_err_refused++;
		} else {
			data->queue_err_unknown++;
		}
	}
}

static
int
print_smtp_header(const char * name) {
	char title[BUFSIZ];

	sprintf(title, "Qmail SMTPD for %s", name);
	nd_chart("qmail", name, "", "smtpd qmail", title, "# smtpd connections",
		"smtpd", "qmail.qmail_smtpd", ND_CHART_TYPE_AREA);
	nd_dimension("tcp_ok",   "TCP OK",   ND_ALG_ABSOLUTE,  1, 1, ND_VISIBLE);
	nd_dimension("tcp_deny", "TCP Deny", ND_ALG_ABSOLUTE, -1, 1, ND_VISIBLE);

	sprintf(title, "Qmail SMTPD Open Sessions for %s", name);
	nd_chart("qmail", name, "status", "smtpd statuses", title,
		"average # sessions", "smtpd", "qmail.qmail_smtpd_status", ND_CHART_TYPE_LINE);
	nd_dimension("tcp_status_average", "session average", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);

	sprintf(title, "Qmail SMTPD End Statuses for %s", name);
	nd_chart("qmail", name, "end_status", "smtpd end statuses", title,
		"# smtpd end statuses", "smtpd", "qmail.qmail_smtpd_end_status", ND_CHART_TYPE_LINE);
	nd_dimension("tcp_end_status_0",      "0",     ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("tcp_end_status_256",    "256",   ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("tcp_end_status_25600",  "25600", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("tcp_end_status_others", "other", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	sprintf(title, "Qmail SMTPD smtp type for %s", name);
	nd_chart("qmail", name, "smtp_type", "smtp type", title, "# smtp protocols",
		"smtpd", "qmail.smtp_type", ND_CHART_TYPE_LINE);
	nd_dimension("smtp",	 "SMTP",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("esmtps", "ESMTPS", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	sprintf(title, "Qmail SMTPD tls connection types for %s", name);
	nd_chart("qmail", name, "tls", "tls version", title, "# tls versions",
		"smtpd", "qmail.qmail_smtpd_tls", ND_CHART_TYPE_LINE);
	nd_dimension("tls1",	 "TLS_1",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("tls1.1",	 "TLS_1.1",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("tls1.2",	 "TLS_1.2",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("tls1.3",	 "TLS_1.3",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("unknown",	 "unknown",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);

	sprintf(title, "Qmail SMTPD qmail-queue error messages for %s", name);
	nd_chart("qmail", name, "queue_err", "", title, "# queue errors",
		"smtpd", "qmail.qmail_smtpd_queue_err", ND_CHART_TYPE_LINE);
	nd_dimension("conn_timeout",	 "conn_timeout",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("comm_failed",	 "comm_failed",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("unprocess",	 "unprocess",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("perm_reject",	 "perm_reject",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("refused",	 "refused",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);
	nd_dimension("unknown",	 "unknown",	 ND_ALG_ABSOLUTE,	1, 1, ND_VISIBLE);

	return fflush(stdout);
}

static
int
print_smtp_data(const char * name, const struct smtp_statistics * data, const unsigned long time) {
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
	nd_set("unknown", data->esmtps_unknown);
	nd_end();

	nd_begin_time("qmail", name, "queue_err", time);
	nd_set("conn_timeout", data->queue_err_conn_timeout);
	nd_set("comm_failed", data->queue_err_comm_failed);
	nd_set("perm_reject", data->queue_err_perm_reject);
	nd_set("refused", data->queue_err_refused);
	nd_set("unprocess", data->queue_err_unprocess);
	nd_set("unknown", data->queue_err_unknown);
	nd_end();

	return fflush(stdout);
}

static
void
clear_smtp_data(struct smtp_statistics * data) {
	int tmp = data->tcp_status;
	memset(data, 0, sizeof * data);
	data->tcp_status = tmp;
}

static
void
postprocess_data(struct smtp_statistics * data) {
	if (data->tcp_status_count)
		data->tcp_status = data->tcp_status_sum * FRACTIONAL_CONVERSION / data->tcp_status_count;
}

static
struct stat_func smtp = {
	.init = &smtp_data_init,
	.fini = &free,

	.print_hdr   = &print_smtp_header,
	.print       = (int (*)(const char *, const void *, unsigned long))&print_smtp_data,
	.process     = (void (*)(const char *, void *))&process_smtp,
	.postprocess = (void (*)(void *))&postprocess_data,
	.clear       = (void (*)(void *))&clear_smtp_data,
};

struct stat_func * smtp_func = &smtp;
