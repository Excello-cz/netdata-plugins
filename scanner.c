/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netdata.h"
#include "callbacks.h"

#include "scanner.h"

/* Netdata collects integer values only. We have to multiply collected value by
 * this constant and set the DIMENSION divider to the same value if we need
 * fractional values.	*/
#define FRACTIONAL_CONVERSION 1000000

struct scanner_statistics {
	int clear;
	int clamdscan;
	int spam_tagged;
	int spam_rejected;
	int spam_deleted;
	int other;

	int sc_0;
	int sc_1;

	int cc_0;
	int cc_1;

	int scan_duration_sc_0_cc_0;
	int scan_duration_sc_0_cc_0_count;
	unsigned int scan_duration_sc_0_cc_0_sum;
	int scan_duration_sc_0_cc_1;
	int scan_duration_sc_0_cc_1_count;
	unsigned int scan_duration_sc_0_cc_1_sum;
	int scan_duration_sc_1_cc_0;
	int scan_duration_sc_1_cc_0_count;
	unsigned int scan_duration_sc_1_cc_0_sum;
	int scan_duration_sc_1_cc_1;
	int scan_duration_sc_1_cc_1_count;
	unsigned int scan_duration_sc_1_cc_1_sum;
	/* only the clamav results, nothing done by scanners */
	int scan_duration_cc_0;
	int scan_duration_cc_0_count;
	unsigned int scan_duration_cc_0_sum;
	int scan_duration_cc_1;
	int scan_duration_cc_1_count;
	unsigned int scan_duration_cc_1_sum;
	/* only the scanner results, nothing done by clamav */
	int scan_duration_sc_0;
	int scan_duration_sc_0_count;
	unsigned int scan_duration_sc_0_sum;
	int scan_duration_sc_1;
	int scan_duration_sc_1_count;
	unsigned int scan_duration_sc_1_sum;
	/* whitelist and the others */
	int scan_duration__;
	int scan_duration__count;
	unsigned int scan_duration__sum;
};

static
void *
scanner_data_init() {
	struct scanner_statistics * ret;
	ret = calloc(1, sizeof * ret);
	return ret;
}

static
void
scanner_clear(struct scanner_statistics * data) {
	memset(data, 0, sizeof * data);
}

static
const char *
get_next_field(char * restrict buf, size_t size, const char * restrict line) {
	const char * s = line;
	char * d = buf;

	for (; *s && *s != '\t' && d - buf < size; d++, s++) {
		*d = *s;
	}

	if (d - buf >= size) {
		return NULL;
	}

	*d = '\0';

	return s;
}

static
void
scanner_process(const char * line, struct scanner_statistics * data) {
	int sc_stat = -1;
	int cc_stat = -1;
	char buf[256];

	/* Skip date */
	if ((line = get_next_field(buf, sizeof buf, line)) == NULL) {
		fprintf(stderr, "scanner.plugin: cannot skip date\n");
		return;
	}

	/* Load scan status */
	if ((line = get_next_field(buf, sizeof buf, line + 1)) == NULL) {
		fprintf(stderr, "scanner.plugin: cannot get status\n");
		return;
	}

	if (strstr(buf, "Clear")) {
		data->clear++;
	} else if (strstr(buf, "CLAMDSCAN")) {
		data->clamdscan++;
	} else if (strstr(buf, ":SPAM-TAGGED")) {
		data->spam_tagged++;
	} else if (strstr(buf, ":SPAM-REJECTED")) {
		data->spam_rejected++;
	} else if (strstr(buf, ":SPAM-DELETED")) {
		data->spam_deleted++;
	} else {
		data->other++;
	}

	if (strstr(buf, ":SC:0")) {
		data->sc_0++;
		sc_stat = 0;
	} else if (strstr(buf, ":SC:1")) {
		data->sc_1++;
		sc_stat = 1;
	}

	if (strstr(buf, ":CC:0")) {
		data->cc_0++;
		cc_stat = 0;
	} else if (strstr(buf, ":CC:1")) {
		data->cc_1++;
		cc_stat = 1;
	}

	/* Load time */
	if ((line = get_next_field(buf, sizeof buf, line + 1)) == NULL) {
		fprintf(stderr, "scanner.plugin: cannot get processing time\n");
		return;
	}

	int duration = atof(buf) * FRACTIONAL_CONVERSION;
	if (sc_stat == -1 && cc_stat == -1) {
		data->scan_duration__count++;
		data->scan_duration__sum += duration;
	}
	else if(sc_stat == -1 && cc_stat == 0) {
		data->scan_duration_cc_0_count++;
		data->scan_duration_cc_0_sum += duration;
	}
	else if(sc_stat == -1 && cc_stat == 1) {
		data->scan_duration_cc_1_count++;
		data->scan_duration_cc_1_sum += duration;
	}
	else if(sc_stat == 0 && cc_stat == -1) {
		data->scan_duration_sc_0_count++;
		data->scan_duration_sc_0_sum += duration;
	}
	else if(sc_stat == 0 && cc_stat == 0) {
		data->scan_duration_sc_0_cc_0_count++;
		data->scan_duration_sc_0_cc_0_sum += duration;
	}
	else if(sc_stat == 0 && cc_stat == 1) {
		data->scan_duration_sc_0_cc_1_count++;
		data->scan_duration_sc_0_cc_1_sum += duration;
	}
	else if(sc_stat == 1 && cc_stat == -1) {
		data->scan_duration_sc_1_count++;
		data->scan_duration_sc_1_sum += duration;
	}
	else if(sc_stat == 1 && cc_stat == 0) {
		data->scan_duration_sc_1_cc_0_count++;
		data->scan_duration_sc_1_cc_0_sum += duration;
	}
	else if(sc_stat == 1 && cc_stat == 1) {
		data->scan_duration_sc_1_cc_1_count++;
		data->scan_duration_sc_1_cc_1_sum += duration;
	}
}

static
void
scanner_print_hdr(const char * name) {
	nd_chart("scannerd", name, "type", "", "", "volume", "scannerd", "scannerd.scannerd_type", ND_CHART_TYPE_STACKED);
	nd_dimension("clear", "Clear", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("clamdscan", "Clamdscan", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("spam_tagged", "SPAM Tagged", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("spam_rejected", "SPAM Rejected", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("spam_deleted", "SPAM Deleted", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("other", "Other", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	nd_chart("scannerd", name, "cached", "", "Cached results", "percentage", "scannerd", "scannerd.scannerd_sc", ND_CHART_TYPE_STACKED);
	nd_dimension("sc_0", "SC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, 1, ND_VISIBLE);
	nd_dimension("sc_1", "SC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, 1, ND_VISIBLE);
	nd_dimension("cc_0", "CC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, 1, ND_VISIBLE);
	nd_dimension("cc_1", "CC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, 1, ND_VISIBLE);

	nd_chart("scannerd", name, "duration", "", "Scan duration", "duration", "scannerd", "scannerd.scannerd_scan_duration", ND_CHART_TYPE_LINE);
	nd_dimension("scan_duration_sc_0_cc_0", "SC:0_CC:0", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_0_cc_1", "SC:0_CC:1", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1_cc_0", "SC:1_CC:0", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1_cc_1", "SC:1_CC:1", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_cc_0", "CC:0", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_cc_1", "CC:1", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_0", "SC:0", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1", "SC:1", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration__", "__", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);

	nd_chart("scannerd", name, "duration_ratio", "", "Scan duration ratio", "percentage", "scannerd", "scannerd.scannerd_scan_duration_ratio", ND_CHART_TYPE_STACKED);
	nd_dimension("scan_duration_sc_0_cc_0", "SC:0_CC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_0_cc_1", "SC:0_CC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1_cc_0", "SC:1_CC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1_cc_1", "SC:1_CC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_cc_0", "CC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_cc_1", "CC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_0", "SC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1", "SC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration__", "__", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);

	fflush(stdout);
}

static
void
scanner_print(const char * name, const struct scanner_statistics * data,
		const unsigned long time) {
	nd_begin_time("scannerd", name, "type", time);
	nd_set("clear", data->clear);
	nd_set("clamdscan", data->clamdscan);
	nd_set("spam_tagged", data->spam_tagged);
	nd_set("spam_rejected", data->spam_rejected);
	nd_set("spam_deleted", data->spam_deleted);
	nd_set("other", data->other);
	nd_end();

	nd_begin_time("scannerd", name, "cached", time);
	nd_set("sc_0", data->sc_0);
	nd_set("sc_1", data->sc_1);
	nd_set("cc_0", data->cc_0);
	nd_set("cc_1", data->cc_1);
	nd_end();

	nd_begin_time("scannerd", name, "duration", time);
	nd_set("scan_duration_sc_0_cc_0", data->scan_duration_sc_0_cc_0);
	nd_set("scan_duration_sc_0_cc_1", data->scan_duration_sc_0_cc_1);
	nd_set("scan_duration_sc_1_cc_0", data->scan_duration_sc_1_cc_0);
	nd_set("scan_duration_sc_1_cc_1", data->scan_duration_sc_1_cc_1);
	nd_set("scan_duration_cc_0", data->scan_duration_cc_0);
	nd_set("scan_duration_cc_1", data->scan_duration_cc_1);
	nd_set("scan_duration_sc_0", data->scan_duration_sc_0);
	nd_set("scan_duration_sc_1", data->scan_duration_sc_1);
	nd_set("scan_duration__", data->scan_duration__);
	nd_end();

	nd_begin_time("scannerd", name, "duration_ratio", time);
	nd_set("scan_duration_sc_0_cc_0", data->scan_duration_sc_0_cc_0);
	nd_set("scan_duration_sc_0_cc_1", data->scan_duration_sc_0_cc_1);
	nd_set("scan_duration_sc_1_cc_0", data->scan_duration_sc_1_cc_0);
	nd_set("scan_duration_sc_1_cc_1", data->scan_duration_sc_1_cc_1);
	nd_set("scan_duration_cc_0", data->scan_duration_cc_0);
	nd_set("scan_duration_cc_1", data->scan_duration_cc_1);
	nd_set("scan_duration_sc_0", data->scan_duration_sc_0);
	nd_set("scan_duration_sc_1", data->scan_duration_sc_1);
	nd_set("scan_duration__", data->scan_duration__);
	nd_end();

	fflush(stdout);
}

static
void
postprocess_data(struct scanner_statistics * data) {
	if (data->scan_duration_sc_0_cc_0_count) {
		data->scan_duration_sc_0_cc_0 = data->scan_duration_sc_0_cc_0_sum / data->scan_duration_sc_0_cc_0_count;
	}
	if (data->scan_duration_sc_0_cc_1_count) {
		data->scan_duration_sc_0_cc_1 = data->scan_duration_sc_0_cc_1_sum / data->scan_duration_sc_0_cc_1_count;
	}
	if (data->scan_duration_sc_1_cc_0_count) {
		data->scan_duration_sc_1_cc_0 = data->scan_duration_sc_1_cc_0_sum / data->scan_duration_sc_1_cc_0_count;
	}
	if (data->scan_duration_sc_1_cc_1_count) {
		data->scan_duration_sc_1_cc_1 = data->scan_duration_sc_1_cc_1_sum / data->scan_duration_sc_1_cc_1_count;
	}
	if (data->scan_duration_cc_0_count) {
		data->scan_duration_cc_0 = data->scan_duration_cc_0_sum / data->scan_duration_cc_0_count;
	}
	if (data->scan_duration_cc_1_count) {
		data->scan_duration_cc_1 = data->scan_duration_cc_1_sum / data->scan_duration_cc_1_count;
	}
	if (data->scan_duration_sc_0_count) {
		data->scan_duration_sc_0 = data->scan_duration_sc_0_sum / data->scan_duration_sc_0_count;
	}
	if (data->scan_duration_sc_1_count) {
		data->scan_duration_sc_1 = data->scan_duration_sc_1_sum / data->scan_duration_sc_1_count;
	}
	if (data->scan_duration__count) {
		data->scan_duration__ = data->scan_duration__sum / data->scan_duration__count;
	}
}

static
struct stat_func scanner = {
	.init = &scanner_data_init,
	.fini = &free,

	.print_hdr   = scanner_print_hdr,
	.print       = (void (*)(const char *, const void *, unsigned long))scanner_print,
	.process     = (void (*)(const char *, void *))scanner_process,
	.postprocess = (void (*)(void *))&postprocess_data,
	.clear       = (void (*)(void *))&scanner_clear,
};

struct stat_func * scanner_func = &scanner;
