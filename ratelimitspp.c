/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netdata.h"
#include "callbacks.h"

#include "ratelimitspp.h"
#include "smtp.h"

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
ratelimitspp_aggregate(struct ratelimitspp_statistics * data, const struct smtp_statistics * smtp_stats) {
	data->conn_timeout+= smtp_stats->ratelimitspp_conn_timeout;
	data->error+= smtp_stats->ratelimitspp_error;
	data->ratelimited+= smtp_stats->ratelimitspp_ratelimited;
}

static
int
ratelimitspp_print_hdr() {
	nd_chart("ratelimitspp", "", "", "", "Table updates by ratelimitspp", "events", "ratelimitspp", "ratelimitspp.events", ND_CHART_TYPE_LINE);
	nd_dimension("conn_timeout", "conn_timeout", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("error", "error", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("ratelimited", "ratelimited", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	return fflush(stdout);
}

static
int
ratelimitspp_print(const struct ratelimitspp_statistics * data,
		const unsigned long time) {
	nd_begin_time("ratelimitspp", "", "table_updates", time);
	nd_set("timeout", data->conn_timeout);
	nd_set("error", data->error);
	nd_set("ratelimited", data->ratelimited);
	nd_end();

	return fflush(stdout);
}

static
struct aggreg_func ratelimitspp = {
	.init = &ratelimitspp_data_init,
	.fini = &free,

	.print_hdr = ratelimitspp_print_hdr,
	.print = (int (*)(const void *, unsigned long))ratelimitspp_print,
	.aggregate = (void (*)(void *, const void *))ratelimitspp_aggregate,
	.postprocess = NULL,
	.clear = (void (*)(void *))&ratelimitspp_clear,
};

struct aggreg_func * ratelimitspp_func = &ratelimitspp;
