/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netdata.h"
#include "callbacks.h"

#include "ratelimitspp.h"

struct ratelimitspp_statistics {
        int conn_timeout;
        int error;
        int ratelimited;
};

static
void *
ratelimitspp_data_init() {
	struct ratelimitspp_statistics * ret;
	ret = calloc(1, sizeof * ret);
	return ret;
}

static
void
ratelimitspp_clear(struct ratelimitspp_statistics * data) {
	memset(data, 0, sizeof * data);
}

void
ratelimitspp_aggregate(const char * line, struct ratelimitspp_statistics * data, const struct smtp_statistics * smtp_stats) {
	data->conn_timeout+= smtp_stats->ratelimitspp_conn_timeout;
	data->error+= smtp_stats->ratelimitspp_error;
	data->ratelimited+= smtp_stats->ratelimitspp_ratelimited;
}

static
int
ratelimitspp_print_hdr(const char * name) {
	nd_chart("ratelimitspp", name, "results", "", "Table updates by ratelimitspp", "update", "ratelimitspp", "ratelimitspp.table_updates", ND_CHART_TYPE_STACKED);
	nd_dimension("conn_timeout", "conn_timeout", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("error", "error", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("ratelimited", "ratelimited", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	return fflush(stdout);
}

static
int
ratelimitspp_print(const char * name, const struct ratelimitspp_statistics * data,
		const unsigned long time) {
	nd_begin_time("ratelimitspp", name, "table_updates", time);
	nd_set("conn_failed", data->conn_failed);
	nd_set("scanner_success", data->scanner_success);
	nd_set("scanner_failed", data->scanner_failed);
	nd_set("delivery_success", data->delivery_success);
	nd_set("delivery_failed", data->delivery_failed);
	nd_set("unknown_success", data->unknown_success);
	nd_set("unknown_failed", data->unknown_failed);
	nd_set("other", data->other);
	nd_end();

	return fflush(stdout);
}

static
struct stat_func ratelimitspp = {
	.init = &ratelimitspp_data_init,
	.fini = &free,

	.print_hdr = ratelimitspp_print_hdr,
	.print = (int (*)(const char *, const void *, unsigned long))ratelimitspp_print,
	.process = NULL,
	.postprocess = NULL,
	.clear = (void (*)(void *))&ratelimitspp_clear,
};

struct stat_func * ratelimitspp_func = &ratelimitspp;
