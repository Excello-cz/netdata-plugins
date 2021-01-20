/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "callbacks.h"
#include "netdata.h"
#include "err.h"
#include "vector.h"

#include "smtp.h"

/* Netdata collects integer values only. We have to multiply collected value by
 * this constant and set the DIMENSION divider to the same value if we need
 * fractional values.  */
#define FRACTIONAL_CONVERSION 100

struct ratelimitspp_statistics {
	int conn_timeout;
	int error;
	int ratelimited;
};

struct limit_t {
	int count;
	char rulename[256];
	int new;
};

struct smtp_statistics_scalar {
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

	struct ratelimitspp_statistics ratelimitspp;
};

struct smtp_statistics_vector {
	struct vector maxconnnet;
	struct vector maxconnip;
	struct vector maxconnrule;
	struct vector maxload;
};

struct smtp_statistics {
	struct smtp_statistics_vector ssv;
	struct smtp_statistics_scalar sss;
};

static
struct
ratelimitspp_statistics aggregated_ratelimtspp;

static
struct
smtp_statistics_vector aggregated_limits;

static
void *
smtp_data_init() {
	struct smtp_statistics * ret;
	ret = calloc(1, sizeof * ret);
	vector_init(&ret->ssv.maxload, sizeof(struct limit_t));
	vector_init(&ret->ssv.maxconnnet, sizeof(struct limit_t));
	vector_init(&ret->ssv.maxconnip, sizeof(struct limit_t));
	vector_init(&ret->ssv.maxconnrule, sizeof(struct limit_t));
	vector_init(&aggregated_limits.maxload, sizeof(struct limit_t));
	vector_init(&aggregated_limits.maxconnnet, sizeof(struct limit_t));
	vector_init(&aggregated_limits.maxconnip, sizeof(struct limit_t));
	vector_init(&aggregated_limits.maxconnrule, sizeof(struct limit_t));
	return ret;
}

static
void
set_rulename(char * name_d, const char * name_s, const size_t size) {
	memset(name_d, 0, size);
	for (int i = 0; i < size - 1; i++) {
		if (name_s[i] == '\0' || name_s[i] == ')') {
			break;
		}
		if (name_s[i] == '.') {
			name_d[i] = '+';
		} else {
			name_d[i] = name_s[i];
		}
	}
}

static
void
update_limit(struct vector * limits, const char * rulename_p) {
	struct limit_t * _limit = 0;
	struct limit_t limit;

	set_rulename(limit.rulename, rulename_p, sizeof limit.rulename);

	for (int i = 0; i < limits->len; i++) {
		_limit = vector_item(limits, i);
		if (strcmp(_limit->rulename, limit.rulename))
			_limit = 0;
	}

	if (_limit) {
		_limit->count++;
	}
	else {
		limit.count = 1;
		vector_add(limits, &limit);
	}
}

static
void
process_smtp(const char * line, struct smtp_statistics * data) {
	char * ptr;
	int val;

	if (strstr(line, "tcpserver: ok")) {
		data->sss.tcp_ok++;
	} else if ((ptr = strstr(line, "tcpserver: deny"))) {
		data->sss.tcp_deny++;
		char * rulename = 0;
		if ((rulename = strstr(ptr, "("))) {
			rulename++;
			if (!rulename)
				fprintf(stderr, "Can't extract rule name on line: %s\n", line);
			else if (strstr(rulename, "MAXLOAD:")) {
				update_limit(&data->ssv.maxload, rulename);
			}
			else if (strstr(rulename, "MAXCONNIP:")) {
				update_limit(&data->ssv.maxconnip, rulename);
			}
			else if (strstr(rulename, "MAXCONNNET:")) {
				update_limit(&data->ssv.maxconnnet, rulename);
			}
			else if (strstr(rulename, "MAXCONNRULE:")) {
				update_limit(&data->ssv.maxconnrule, rulename);
			}
		}
	} else if ((ptr = strstr(line, "tcpserver: status: "))) {
		val = strtoul(ptr + sizeof "tcpserver: status: " - 1, 0, 0);
		data->sss.tcp_status_sum += val;
		data->sss.tcp_status_count++;
	} else if ((ptr = strstr(line, "tcpserver: end "))) {
		ptr = strstr(ptr, "status ");
		if (ptr) {
			val = strtoul(ptr + sizeof "status " - 1, 0, 0);
			switch (val) {
			case 0:
				data->sss.tcp_end_status_0++;
				break;
			case 256:
				data->sss.tcp_end_status_256++;
				break;
			case 25600:
				data->sss.tcp_end_status_25600++;
				break;
			default:
				data->sss.tcp_end_status_others++;
				break;
			}
		}
	} else if ((ptr = strstr(line, "uses ESMTPS"))) {
		data->sss.esmtps++;
		if (strstr(ptr, "TLSv1,")) {
			data->sss.esmtps_tls_1++;
		} else if (strstr(ptr, "TLSv1.1,")) {
			data->sss.esmtps_tls_1_1++;
		} else if (strstr(ptr, "TLSv1.2,")) {
			data->sss.esmtps_tls_1_2++;
		} else if (strstr(ptr, "TLSv1.3,")) {
			data->sss.esmtps_tls_1_3++;
		} else {
			data->sss.esmtps_unknown++;
		}
	} else if ((ptr = strstr(line, "uses SMTP"))) {
		data->sss.smtp++;
	} else if ((ptr = strstr(line, "qmail-smtpd: qmail-queue error message: "))) {
		if (strstr(ptr, "451 tcp connection to mail server timed out")) {
			data->sss.queue_err_conn_timeout++;
		} else if (strstr(ptr, "451 unable to process message")) {
			data->sss.queue_err_unprocess++;
		} else if (strstr(ptr, "451 tcp connection to mail server succeeded, but communication failed")) {
			data->sss.queue_err_comm_failed++;
		} else if (strstr(ptr, "554 mail server permanently rejected message")) {
			data->sss.queue_err_perm_reject++;
		} else if (strstr(ptr, "554 message refused")) {
			data->sss.queue_err_refused++;
		} else {
			data->sss.queue_err_unknown++;
		}
	} else if ((ptr = strstr(line, "ratelimitspp:"))) {
		if (strstr(ptr, ";Result:NOK")) {
			data->sss.ratelimitspp.ratelimited++;
		} else if ((ptr = strstr(ptr, "Error:"))) {
			if (strstr(ptr, "Receiving data failed, connection timed out.")) {
				data->sss.ratelimitspp.conn_timeout++;
			} else {
				data->sss.ratelimitspp.error++;
			}
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
	nd_set("tcp_ok", data->sss.tcp_ok);
	nd_set("tcp_deny", data->sss.tcp_deny);
	nd_end();

	nd_begin_time("qmail", name, "status", time);
	nd_set("tcp_status_average", data->sss.tcp_status);
	nd_end();

	nd_begin_time("qmail", name, "end_status", time);
	nd_set("tcp_end_status_0", data->sss.tcp_end_status_0);
	nd_set("tcp_end_status_256", data->sss.tcp_end_status_256);
	nd_set("tcp_end_status_25600", data->sss.tcp_end_status_25600);
	nd_set("tcp_end_status_others", data->sss.tcp_end_status_others);
	nd_end();

	nd_begin_time("qmail", name, "smtp_type", time);
	nd_set("smtp", data->sss.smtp);
	nd_set("esmtps", data->sss.esmtps);
	nd_end();

	nd_begin_time("qmail", name, "tls", time);
	nd_set("tls1", data->sss.esmtps_tls_1);
	nd_set("tls1.1", data->sss.esmtps_tls_1_1);
	nd_set("tls1.2", data->sss.esmtps_tls_1_2);
	nd_set("tls1.3", data->sss.esmtps_tls_1_3);
	nd_set("unknown", data->sss.esmtps_unknown);
	nd_end();

	nd_begin_time("qmail", name, "queue_err", time);
	nd_set("conn_timeout", data->sss.queue_err_conn_timeout);
	nd_set("comm_failed", data->sss.queue_err_comm_failed);
	nd_set("perm_reject", data->sss.queue_err_perm_reject);
	nd_set("refused", data->sss.queue_err_refused);
	nd_set("unprocess", data->sss.queue_err_unprocess);
	nd_set("unknown", data->sss.queue_err_unknown);
	nd_end();
	return fflush(stdout);
}

static
void
clear_limits(struct vector * limit) {
	struct limit_t * l = 0;
	for (int i = 0; i < limit->len; i++) {
		l = vector_item(limit, i);
		l->count = 0;
	}
}

static
void
clear_smtp_data(struct smtp_statistics * data) {
	int tmp = data->sss.tcp_status;
	memset(&data->sss, 0, sizeof data->sss);
	data->sss.tcp_status = tmp;
	clear_limits(&data->ssv.maxload);
	clear_limits(&data->ssv.maxconnip);
	clear_limits(&data->ssv.maxconnnet);
	clear_limits(&data->ssv.maxconnrule);
}

static
void
postprocess_limits(struct vector * limit_aggregated, struct vector * limit) {
	struct limit_t * l = 0;
	struct limit_t * l_a = 0;

	for (int i = 0; i < limit->len; i++) {
		l = vector_item(limit, i);
		int found = 0;
		for (int i_a = 0; i_a < limit_aggregated->len; i_a++) {
			l_a = vector_item(limit_aggregated, i_a);
			if (!strcmp(l->rulename, l_a->rulename)) {
				l_a->count += l->count;
				found = 1;
				break;
			}
		}
		if (!found) {
			l->new = 1;
			vector_add(limit_aggregated, l);
		}
	}
}

static
void
postprocess_data(struct smtp_statistics * data) {
	if (data->sss.tcp_status_count)
		data->sss.tcp_status = data->sss.tcp_status_sum * FRACTIONAL_CONVERSION / data->sss.tcp_status_count;

	aggregated_ratelimtspp.conn_timeout += data->sss.ratelimitspp.conn_timeout;
	aggregated_ratelimtspp.error += data->sss.ratelimitspp.error;
	if (data->sss.ratelimitspp.ratelimited)
		aggregated_ratelimtspp.ratelimited = 1;

	postprocess_limits(&aggregated_limits.maxload, &data->ssv.maxload);
	postprocess_limits(&aggregated_limits.maxconnip, &data->ssv.maxconnip);
	postprocess_limits(&aggregated_limits.maxconnnet, &data->ssv.maxconnnet);
	postprocess_limits(&aggregated_limits.maxconnrule, &data->ssv.maxconnrule);
}

static
void
finish (struct smtp_statistics * data) {
	vector_free(&data->ssv.maxconnnet);
	vector_free(&data->ssv.maxconnip);
	vector_free(&data->ssv.maxconnrule);
	vector_free(&data->ssv.maxload);
	free(data);
}

static
struct stat_func smtp = {
	.init = &smtp_data_init,
	.fini = (void (*)(void *))&finish,

	.print_hdr   = &print_smtp_header,
	.print       = (int (*)(const char *, const void *, unsigned long))&print_smtp_data,
	.process     = (void (*)(const char *, void *))&process_smtp,
	.postprocess = (void (*)(void *))&postprocess_data,
	.clear       = (void (*)(void *))&clear_smtp_data,
};

struct stat_func * smtp_func = &smtp;

void
ratelimitspp_clear() {
	memset(&aggregated_ratelimtspp, 0, sizeof aggregated_ratelimtspp);
}

void
tcpserverlimits_clear() {
	clear_limits(&aggregated_limits.maxload);
	clear_limits(&aggregated_limits.maxconnip);
	clear_limits(&aggregated_limits.maxconnnet);
	clear_limits(&aggregated_limits.maxconnrule);
}

int
ratelimitspp_print_hdr() {
	nd_chart("qmail", "ratelimitspp", "events", "", "events of ratelimitspp", "events", "ratelimitspp", "ratelimitspp.events", ND_CHART_TYPE_LINE);
	nd_dimension("conn_timeout", "conn_timeout", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("error", "error", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("ratelimited", "ratelimited", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	return fflush(stdout);
}

int
ratelimitspp_print(const unsigned long time) {
	nd_begin_time("qmail", "ratelimitspp", "events", time);
	nd_set("conn_timeout", aggregated_ratelimtspp.conn_timeout);
	nd_set("error", aggregated_ratelimtspp.error);
	nd_set("ratelimited", aggregated_ratelimtspp.ratelimited);
	nd_end();
	return fflush(stdout);
}

static
void
print_limits(struct vector * limit, const char * limit_name, const unsigned long time) {
	struct limit_t * l;

	struct vector newdim;
	vector_init(&newdim, sizeof(struct limit_t));

	for (int i = 0; i < limit->len; i++) {
		l = vector_item(limit, i);
		if (l->new) {
			vector_add(&newdim, l);
		}
	}

	if (limit->len > newdim.len) {
		nd_begin_time("qmail", "limit", limit_name, time);
		for (int i = 0; i < limit->len; i++) {
			l = vector_item(limit, i);
			if (l->new) {
				l->new = 0;
			} else {
				nd_set(l->rulename, l->count);
			}
		}
		nd_end();
	}

	for (int i = 0; i < newdim.len; i++) {
		l = vector_item(&newdim, i);
		char title[BUFSIZ];
		sprintf(title, "Qmail SMTPD %s limit", limit_name);
		nd_chart("qmail", "limit", limit_name, "", title, "# reaches",
			"tcpserver", "qmail.qmail_smtpd_limits", ND_CHART_TYPE_LINE);
		nd_dimension(l->rulename, l->rulename,     ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

		nd_begin_time("qmail", "limit", limit_name, time);
		nd_set(l->rulename, l->count);
		nd_end();
		if (limit->len == newdim.len) {
			l = vector_item(limit, i);
			l->new = 0;
		}
	}
	vector_free(&newdim);

}

int
tcpserverlimits_print(const unsigned long time) {
	print_limits(&aggregated_limits.maxload, "maxload", time);
	print_limits(&aggregated_limits.maxconnip, "maxconnip", time);
	print_limits(&aggregated_limits.maxconnrule, "maxconnrule", time);
	print_limits(&aggregated_limits.maxconnnet, "maxconnnet", time);
	return fflush(stdout);
}
