/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netdata.h"
#include "callbacks.h"

#include "parser.h"

struct parser_statistics {
	int conn_failed;
	int scanner_success;
	int scanner_failed;
	int delivery_success;
	int delivery_failed;
	int unknown_success;
	int unknown_failed;
	int other;
};

static
void *
parser_data_init() {
	struct parser_statistics * ret;
	ret = calloc(1, sizeof * ret);
	return ret;
}

static
void
parser_clear(struct parser_statistics * data) {
	memset(data, 0, sizeof * data);
}

static
void
parser_process(const char * line, struct parser_statistics * data) {
	const char * ptr;
	if ((ptr = strstr(line, "Successfully updated table "))) {
		if (strstr(ptr, "scanner")) {
			data->scanner_success++;
		}
		else if (strstr(ptr, "delivery")) {
			data->delivery_success++;
		}
		else {
			data->unknown_success++;
		}
	} else if ((ptr = strstr(line, "Failed to update table "))) {
	 if (strstr(ptr, "scanner")) {
			data->scanner_failed++;
		}
		else if (strstr(ptr, "delivery")) {
			data->delivery_failed++;
		}
		else {
			data->unknown_failed++;
		}
	} else if ((ptr = strstr(line, "Can't connect to MySQL server on "))) {
    if (strstr(ptr, "[Errno 111] Connection refused")) {
      data->conn_failed++;
    }
  } else {
		data->other++;
	}
}

static
void
parser_print_hdr(const char * name) {
	nd_chart("parser", name, "table_updates", "", "Table updates by parser", "update", "parser", "parser.table_updates", ND_CHART_TYPE_STACKED);
  nd_dimension("conn_failed", "conn_failed", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
  nd_dimension("scanner_success", "scanner_success", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
  nd_dimension("scanner_failed", "scanner_failed", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
  nd_dimension("delivery_success", "delivery_success", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
  nd_dimension("delivery_failed", "delivery_failed", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
  nd_dimension("unknown_success", "unknown_success", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
  nd_dimension("unknown_failed", "unknown_failed", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
  nd_dimension("other", "other", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	fflush(stdout);
}

static
void
parser_print(const char * name, const struct parser_statistics * data,
		const unsigned long time) {
	nd_begin_time("parser", name, "table_updates", time);
	nd_set("conn_failed", data->conn_failed);
	nd_set("scanner_success", data->scanner_success);
	nd_set("scanner_failed", data->scanner_failed);
	nd_set("delivery_success", data->delivery_success);
	nd_set("delivery_failed", data->delivery_failed);
	nd_set("unknown_success", data->unknown_success);
	nd_set("unknown_failed", data->unknown_failed);
	nd_set("other", data->other);
	nd_end();

  fflush(stdout);
}

static
struct stat_func parser = {
	.init = &parser_data_init,
	.fini = &free,

	.print_hdr = parser_print_hdr,
	.print = (void (*)(const char *, const void *, unsigned long))parser_print,
	.process = (void (*)(const char *, void *))parser_process,
	.postprocess = NULL,
	.clear = (void (*)(void *))&parser_clear,
};

struct stat_func * parser_func = &parser;
