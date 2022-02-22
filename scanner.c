/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netdata.h"
#include "callbacks.h"
#include "err.h"
#include "vector.h"

#include "scanner.h"

/* Netdata collects integer values only. We have to multiply collected value by
 * this constant and set the DIMENSION divider to the same value if we need
 * fractional values.	*/
#define FRACTIONAL_CONVERSION 1000000

/* Number of fields in the details log  */
#define NUM_OF_FIELDS 15

struct details_statistics {
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

	unsigned int null_field;
	unsigned int empty_field;
	unsigned int incorrectNumOfClmns;
};

#define SIZE_OF_WARN_NAME 16
struct warn_t {
	int count;
	char name[SIZE_OF_WARN_NAME];
	int new;
};

struct scannerd_statistics_scalar {
// Errors
	unsigned int ex_attempts;
	unsigned int ex_scantimeout;
	unsigned int ex_unexpdata;
	unsigned int ex_unknown;
	unsigned int ex_mime_err;
	unsigned int ex_archive_err;

	unsigned int rs_badresponse;

	unsigned int scan_res;
	unsigned int scan_wl_query;
	unsigned int scan_wl_scanner;
	unsigned int scan_qmqpc_rule;
	unsigned int scan_mess;
	unsigned int scan_clean;

	unsigned int daemon_scanner_repl;
	unsigned int daemon_conn;
	unsigned int daemon_connhandle;

	unsigned int unpack_file_output;
	unsigned int unpack_file_err;
	unsigned int unpack_deldir;
	unsigned int unpack_delfile;
	unsigned int unpack_del;

// Warning
	unsigned int ex_maxsize;

	unsigned int scan_unknown_wl_reply;

	unsigned int daemon_conn_closed;
};

struct scannerd_statistics {
	// Warnings vector dimensions
	struct vector swv;
	struct scannerd_statistics_scalar sss;
};

static
void *
details_data_init() {
	struct details_statistics * ret;
	ret = calloc(1, sizeof * ret);
	return ret;
}

static
void *
scannerd_data_init() {
	struct scannerd_statistics * ret;
	ret = calloc(1, sizeof * ret);
	vector_init(&ret->swv, sizeof(struct warn_t));
	return ret;
}

static
void
warn_clear(struct vector * warn) {
	struct warn_t * l = 0;
	for (int i = 0; i < warn->len; i++) {
		l = vector_item(warn, i);
		l->count = 0;
	}
}

static
void
scannerd_clear(struct scannerd_statistics * data) {
	memset(&data->sss, 0, sizeof data->sss);
	warn_clear(&data->swv);
}

static
void
details_clear(struct details_statistics * data) {
	memset(data, 0, sizeof * data);
}

static
const char *
get_next_field(char * restrict buf, size_t size, const char * restrict line, const char delim) {
	const char * s = line;
	char * d = buf;

	for (; *s && *s != delim && d - buf < size; d++, s++) {
		*d = *s;
	}

	if (d - buf < size) {
		*d = '\0';
	} else {
		d[size - 1] = '\0';
	}

	for (; *s != delim; s++) {
		if (!*s) return NULL;
	}

	return s;
}

static
void
details_process(const char * line, struct details_statistics * data) {
	int sc_stat = -1;
	int cc_stat = -1;
	char buf[256];

	/* Skip date */
	if ((line = get_next_field(buf, sizeof buf, line, '\t')) == NULL) {
		data->incorrectNumOfClmns = 1;
		return;
	}

	/* Load scan status */
	if ((line = get_next_field(buf, sizeof buf, line + 1, '\t')) == NULL) {
		data->incorrectNumOfClmns = 1;
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
	if ((line = get_next_field(buf, sizeof buf, line + 1, '\t')) == NULL) {
		data->incorrectNumOfClmns = 1;
		return;
	}

	int duration = atof(buf) * FRACTIONAL_CONVERSION;
	if (sc_stat == -1 && cc_stat == -1) {
		data->scan_duration__count++;
		data->scan_duration__sum += duration;
	} else if (sc_stat == -1 && cc_stat == 0) {
		data->scan_duration_cc_0_count++;
		data->scan_duration_cc_0_sum += duration;
	} else if (sc_stat == -1 && cc_stat == 1) {
		data->scan_duration_cc_1_count++;
		data->scan_duration_cc_1_sum += duration;
	} else if (sc_stat == 0 && cc_stat == -1) {
		data->scan_duration_sc_0_count++;
		data->scan_duration_sc_0_sum += duration;
	} else if (sc_stat == 0 && cc_stat == 0) {
		data->scan_duration_sc_0_cc_0_count++;
		data->scan_duration_sc_0_cc_0_sum += duration;
	} else if (sc_stat == 0 && cc_stat == 1) {
		data->scan_duration_sc_0_cc_1_count++;
		data->scan_duration_sc_0_cc_1_sum += duration;
	} else if (sc_stat == 1 && cc_stat == -1) {
		data->scan_duration_sc_1_count++;
		data->scan_duration_sc_1_sum += duration;
	} else if (sc_stat == 1 && cc_stat == 0) {
		data->scan_duration_sc_1_cc_0_count++;
		data->scan_duration_sc_1_cc_0_sum += duration;
	} else if (sc_stat == 1 && cc_stat == 1) {
		data->scan_duration_sc_1_cc_1_count++;
		data->scan_duration_sc_1_cc_1_sum += duration;
	}

	/* Just one detected error is enough for an evidence and eventual alert */
	int cur_field_num = 4;
	for (; cur_field_num <= NUM_OF_FIELDS; cur_field_num++) {
		line = get_next_field(buf, sizeof buf, line + 1, '\t');
		if (cur_field_num < NUM_OF_FIELDS && line == NULL) {
			data->incorrectNumOfClmns = 1;
			return;
		} else if (cur_field_num >= 11) {
			if (!*buf) {
				data->empty_field = 1;
				return;
			} else if (strstr(buf, "NULL")) {
				data->null_field = 1;
				return;
			}
		}
	}
	if (line != NULL) {
		data->incorrectNumOfClmns = 1;
	}
}

#define STARTSWITH(str, start) (strncmp(start, str, sizeof start - 1) == 0)

static
int
get_conn_ip(const char * line, char * ip_lastpart) {
	#define UNTOCONN "unable to connect to "
	char ip[64];

	ip_lastpart[0] = '\0';

	if (!STARTSWITH(line, UNTOCONN)) return 0;
	if ((line = get_next_field(ip, sizeof ip, line + sizeof UNTOCONN - 1, ' ')) == NULL) {
		return 0;
	}

	// Get rid of the last '"' and ':'
	int ipend = strlen(ip) - 3;

	if (ipend < 4) return 1;

	int ipstart = ipend;
	for (; ipstart > ipend - 4 && ip[ipstart] != '.' && ip[ipstart] != ':'; ipstart--);

	int ip_lastpart_len = ipend - ipstart;
	ipstart++;
	memcpy(ip_lastpart, ip + ipstart, ip_lastpart_len);
	ip_lastpart[ip_lastpart_len] = '\0';
	return 1;
}

static
int
get_scan_ip(const char * line, char * ip_lastpart) {
	#define SCANWITH "scanning with "
	char ip[64];

	ip_lastpart[0] = '\0';

	if (!STARTSWITH(line, SCANWITH)) return 0;
	if ((line = get_next_field(ip, sizeof ip, line + sizeof SCANWITH - 1, ' ')) == NULL) {
		return 0;
	}

	// Get rid of the last '"'
	int ipend = strlen(ip) - 2;

	if (ipend < 4) return 1;

	int ipstart = ipend;
	for (; ipstart > ipend - 4 && ip[ipstart] != '.' && ip[ipstart] != ':'; ipstart--);

	int ip_lastpart_len = ipend - ipstart;
	ipstart++;
	memcpy(ip_lastpart, ip + ipstart, ip_lastpart_len);
	ip_lastpart[ip_lastpart_len] = '\0';
	return 1;
}

#define SCANNERNAME_LEN 2
#define WARNT_LEN 4
static
void
add_warn(const char * scanner, const char * warnt, const char * ip, struct vector * warn) {
	char name [SIZE_OF_WARN_NAME];
	int name_len = SCANNERNAME_LEN;
	int ip_len = strlen(ip);

	memcpy(name, scanner, name_len);
	name[name_len] = '_';
	name_len++;
	memcpy(name + name_len, warnt, WARNT_LEN);
	name_len += WARNT_LEN;
	name[name_len] = '_';
	name_len++;
	if (ip_len) {
		memcpy(name + name_len, ip, ip_len);
		name_len += ip_len;
	} else {
		name[name_len] = '?';
		name_len++;
	}

	name[name_len] = '\0';

	int found = 0;

	for (int i = 0; i < warn->len; i++) {
		struct warn_t * w;
		w = vector_item(warn, i);
		if (!strcmp(name, w->name)) {
			w->count++;
			found = 1;
			break;
		}
	}

	if (!found) {
		struct warn_t w;
		w.new = 1;
		w.count = 1;
		memcpy(w.name, name, name_len + 1);
		vector_add(warn, &w);
	}
}

static
void
scannerd_process(const char * line, struct scannerd_statistics * data) {
	char buf[1];
	char ip[4];
	const char * severity;
	const char * module;
	const char * log;

	/* Skip date */
	if ((severity = get_next_field(buf, sizeof buf, line, ' ')) == NULL) {
		fprintf(stderr, "scanner.plugin: cannot get severity\n");
		return;
	}
	if ((module = get_next_field(buf, sizeof buf, severity + 1, ' ')) == NULL) {
		fprintf(stderr, "scanner.plugin: cannot get module\n");
		return;
	}
	if ((log = get_next_field(buf, sizeof buf, module + 1, ' ')) == NULL) {
		fprintf(stderr, "scanner.plugin: cannot get status\n");
		return;
	}
	severity++;
	module++;
	log++;
	if (STARTSWITH(severity, "warning:")) {
		if (STARTSWITH(module, "extractor(")) {
			if (STARTSWITH(log, "skipped maxsize")) {
				data->sss.ex_maxsize++;
			} else if (get_conn_ip(log, ip)) {
				add_warn("ex", "conn", ip, &data->swv);
			} else if (get_scan_ip(log, ip)) {
				add_warn("ex", "scan", ip, &data->swv);
			}
		} else if (STARTSWITH(module, "rspamd(")) {
			if (get_conn_ip(log, ip)) {
				add_warn("rs", "conn", ip, &data->swv);
			} else if (get_scan_ip(log, ip)) {
				add_warn("rs", "scan", ip, &data->swv);
			}
		} else if (STARTSWITH(module, "spamassassin")) {
			if (get_conn_ip(log, ip)) {
				add_warn("sa", "conn", ip, &data->swv);
			} else if (get_scan_ip(log, ip)) {
				add_warn("sa", "scan", ip, &data->swv);
			}
		} else if (STARTSWITH(module, "clamav(")) {
			if (get_conn_ip(log, ip)) {
				add_warn("av", "conn", ip, &data->swv);
			} else if (get_scan_ip(log, ip)) {
				add_warn("av", "scan", ip, &data->swv);
			}
		} else if (STARTSWITH(module, "daemon(")) {
			if (strstr(log, "connection closed")) {
				data->sss.daemon_conn_closed++;
			}
		} else if (STARTSWITH(module, "scanner(")) {
			if (strstr(log, "unknown whitelist reply for result ")) {
				data->sss.scan_unknown_wl_reply++;
			}
		}
	} else if (STARTSWITH(severity, "error:")) {
		if (STARTSWITH(module, "extractor(")) {
			if (STARTSWITH(log, "remote extraction attempts failed")) {
				data->sss.ex_attempts++;
			} else if (STARTSWITH(log, "scanning process timed out")) {
				data->sss.ex_scantimeout++;
			} else if (STARTSWITH(log, "unexpected data received: ")) {
				data->sss.ex_unexpdata++;
			} else if (STARTSWITH(log, "unknown: ")) {
				data->sss.ex_unknown++;
			} else if (STARTSWITH(log, "unable to process eml with mime structure ")) {
				data->sss.ex_mime_err++;
			} else if (STARTSWITH(log, "archive error ")) {
				data->sss.ex_archive_err++;
			}
		} else if (STARTSWITH(module, "rspamd(")) {
			if (STARTSWITH(log, "unable to parse rspamd response: ")) {
				data->sss.rs_badresponse++;
			}
		} else if (STARTSWITH(module, "daemon")) {
			if (STARTSWITH(log, "invalid scanner reply: ")) {
				data->sss.daemon_scanner_repl++;
			} else if (STARTSWITH(log, "connection error: ")) {
				data->sss.daemon_conn++;
			} else if (STARTSWITH(log, "unable to handle connection: ")) {
				data->sss.daemon_connhandle++;
			}
		} else if (STARTSWITH(module, "unpacker(")) {
			if (STARTSWITH(log, "invalid file output: ")) {
				data->sss.unpack_file_output++;
			} else if (STARTSWITH(log, "file error ")) {
				data->sss.unpack_file_err++;
			} else if (STARTSWITH(log, "unable to delete directory ")) {
				data->sss.unpack_deldir++;
			} else if (STARTSWITH(log, "unable to delete file ")) {
				data->sss.unpack_delfile++;
			} else if (STARTSWITH(log, "unable to delete: ")) {
				data->sss.unpack_del++;
			}
		} else if (STARTSWITH(module, "scanner(")) {
			if (STARTSWITH(log, "DNS query to whitelist zone ")) {
				data->sss.scan_wl_query++;
			} else if (STARTSWITH(log, "unable to whitelist scanner ")) {
				data->sss.scan_wl_scanner++;
			} else if (STARTSWITH(log, "qmqpc_action: invalid rule ")) {
				data->sss.scan_qmqpc_rule++;
			} else if (STARTSWITH(log, "unable to process message: ")) {
				data->sss.scan_mess++;
			} else if (STARTSWITH(log, "unable to clean: ")) {
				data->sss.scan_clean++;
			} else if (strstr(log, " result: ")) {
				data->sss.scan_res++;
			}
		}
	}
}

static
int
details_print_hdr(const char * name) {
	nd_chart("scannerd", name, "type", "", "", "volume", "details", "details.details_type", ND_CHART_TYPE_STACKED);
	nd_dimension("clear", "Clear", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("clamdscan", "Clamdscan", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("spam_tagged", "SPAM Tagged", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("spam_rejected", "SPAM Rejected", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("spam_deleted", "SPAM Deleted", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("other", "Other", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	nd_chart("scannerd", name, "cached", "", "Cached results", "percentage", "details", "details.details_sc", ND_CHART_TYPE_STACKED);
	nd_dimension("sc_0", "SC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, 1, ND_VISIBLE);
	nd_dimension("sc_1", "SC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, 1, ND_VISIBLE);
	nd_dimension("cc_0", "CC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, 1, ND_VISIBLE);
	nd_dimension("cc_1", "CC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, 1, ND_VISIBLE);

	nd_chart("scannerd", name, "duration", "", "Scan duration", "duration", "details", "details.details_scan_duration", ND_CHART_TYPE_LINE);
	nd_dimension("scan_duration_sc_0_cc_0", "SC:0_CC:0", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_0_cc_1", "SC:0_CC:1", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1_cc_0", "SC:1_CC:0", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1_cc_1", "SC:1_CC:1", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_cc_0", "CC:0", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_cc_1", "CC:1", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_0", "SC:0", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1", "SC:1", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration__", "__", ND_ALG_ABSOLUTE, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);

	nd_chart("scannerd", name, "duration_ratio", "", "Scan duration ratio", "percentage", "details", "details.details_scan_duration_ratio", ND_CHART_TYPE_STACKED);
	nd_dimension("scan_duration_sc_0_cc_0", "SC:0_CC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_0_cc_1", "SC:0_CC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1_cc_0", "SC:1_CC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1_cc_1", "SC:1_CC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_cc_0", "CC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_cc_1", "CC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_0", "SC:0", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration_sc_1", "SC:1", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);
	nd_dimension("scan_duration__", "__", ND_ALG_PERCENTAGE_OF_ABSOLUTE_ROW, 1, FRACTIONAL_CONVERSION, ND_VISIBLE);

	nd_chart("scannerd", name, "incorrect_data_fields", "", "Incorrect data fields", "volume", "details", "details.details_incorrect_data_fields", ND_CHART_TYPE_LINE);
	nd_dimension("null_field", "nullDataField", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("empty_field", "emptyDataField", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("incorrect_#clmns", "incorrect_#clmns", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	return fflush(stdout);
}

static
int
scannerd_print_hdr(const char * name) {
	nd_chart("scannerd", name, "errors", "", "Errors", "# errors", "scannerd", "scannerd.current_errors", ND_CHART_TYPE_LINE);
	nd_dimension("ex_attempts", "ex_attempts", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("ex_scantimeout", "ex_scantimeout", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("ex_unexpdata", "ex_unexpdata", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("ex_unknown", "ex_unknown", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("ex_mime_err", "ex_mime_err", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("ex_archive_err", "ex_archive_err", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("rs_badresponse", "rs_badresponse", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("scan_res", "scan_res", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("scan_wl_query", "scan_wl_query", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("scan_wl_scanner", "scan_wl_scanner", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("scan_qmqpc_rule", "scan_qmqpc_rule", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("scan_mess", "scan_mess", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("scan_clean", "scan_clean", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("daemon_scanner_repl", "daemon_scanner_repl", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("daemon_conn", "daemon_conn", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("daemon_connhandle", "daemon_connhandle", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("unpack_file_output", "unpack_file_output", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("unpack_file_err", "unpack_file_err", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("unpack_deldir", "unpack_deldir", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("unpack_delfile", "unpack_delfile", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("unpack_del", "unpack_del", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	nd_chart("scannerd", name, "warnings", "", "Warnings", "# warnings", "scannerd", "scannerd.current_warnings", ND_CHART_TYPE_LINE);
	nd_dimension("ex_maxsize", "ex_maxsize", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("scan_unknown_wl_reply", "scan_unknown_wl_reply", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("daemon_conn_closed", "daemon_conn_closed", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	return fflush(stdout);
}

static
int
details_print(const char * name, const struct details_statistics * data,
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

	nd_begin_time("scannerd", name, "incorrect_data_fields", time);
	nd_set("null_field", data->null_field);
	nd_set("empty_field", data->empty_field);
	nd_set("incorrect_#clmns", data->incorrectNumOfClmns);
	nd_end();

	return fflush(stdout);
}

static
void
print_warn(const char * dirname, struct vector * warn, const unsigned long time) {
	struct warn_t * w;

	struct vector newdim;
	vector_init(&newdim, sizeof(struct warn_t));

	for (int i = 0; i < warn->len; i++) {
		w = vector_item(warn, i);
		if (w->new) {
			vector_add(&newdim, w);
		}
	}

	if (warn->len > newdim.len) {
		nd_begin_time("scannerd", dirname, "warnings", time);
		for (int i = 0; i < warn->len; i++) {
			w = vector_item(warn, i);
			if (w->new) {
				w->new = 0;
			} else {
				nd_set(w->name, w->count);
			}
		}
		nd_end();
	}

	for (int i = 0; i < newdim.len; i++) {
		w = vector_item(&newdim, i);
		nd_chart("scannerd", dirname, "warnings", "", "Warnings", "# warnings", "scannerd", "scannerd.current_warnings", ND_CHART_TYPE_LINE);
		nd_dimension(w->name, w->name, ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

		nd_begin_time("scannerd", dirname, "warnings", time);
		nd_set(w->name, w->count);
		nd_end();
		if (warn->len == newdim.len) {
			w = vector_item(warn, i);
			w->new = 0;
		}
	}
	vector_free(&newdim);
}

static
void
print_new_header_warn(const char * name, struct vector * warn,
		const unsigned long time) {
	struct warn_t * w;
	for (int i = warn->len - 1; i >= 0; i--) {
		w = vector_item(warn, i);
		if (!w->new) break;

		w->new = 0;
		nd_chart("scannerd", name, "warnings", "", "Warnings", "# warnings", "scannerd", "scannerd.current_warnings", ND_CHART_TYPE_LINE);
		nd_dimension(w->name, w->name, ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	}
}

static
void
nd_set_warn(struct vector * warn) {
	struct warn_t * w;
	for (int i = 0; i < warn->len; i++) {
		w = vector_item(warn, i);
		nd_set(w->name, w->count);
	}
}

static
int
scannerd_print(const char * name, const struct scannerd_statistics * data,
		const unsigned long time) {
	print_new_header_warn(name, &data->swv, time);
	nd_begin_time("scannerd", name, "warnings", time);
	nd_set("ex_maxsize", data->sss.ex_maxsize);
	nd_set("scan_unknown_wl_reply", data->sss.scan_unknown_wl_reply);
	nd_set("daemon_conn_closed", data->sss.daemon_conn_closed);
	nd_set_warn(&data->swv);
	nd_end();

	nd_begin_time("scannerd", name, "errors", time);
	nd_set("ex_attempts", data->sss.ex_attempts);
	nd_set("ex_scantimeout", data->sss.ex_scantimeout);
	nd_set("ex_unexpdata", data->sss.ex_unexpdata);
	nd_set("ex_unknown", data->sss.ex_unknown);
	nd_set("ex_mime_err", data->sss.ex_mime_err);
	nd_set("ex_archive_err", data->sss.ex_archive_err);
	nd_set("rs_badresponse", data->sss.rs_badresponse);
	nd_set("scan_res", data->sss.scan_res);
	nd_set("scan_wl_query", data->sss.scan_wl_query);
	nd_set("scan_wl_scanner", data->sss.scan_wl_scanner);
	nd_set("scan_qmqpc_rule", data->sss.scan_qmqpc_rule);
	nd_set("scan_mess", data->sss.scan_mess);
	nd_set("scan_clean", data->sss.scan_clean);
	nd_set("daemon_scanner_repl", data->sss.daemon_scanner_repl);
	nd_set("daemon_conn", data->sss.daemon_conn);
	nd_set("daemon_connhandle", data->sss.daemon_connhandle);
	nd_set("unpack_file_output", data->sss.unpack_file_output);
	nd_set("unpack_file_err", data->sss.unpack_file_err);
	nd_set("unpack_deldir", data->sss.unpack_deldir);
	nd_set("unpack_delfile", data->sss.unpack_delfile);
	nd_set("unpack_del", data->sss.unpack_del);
	nd_end();

	return fflush(stdout);
}

static
void
details_postprocess(struct details_statistics * data) {
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
struct stat_func details = {
	.init = &details_data_init,
	.fini = &free,

	.print_hdr   = details_print_hdr,
	.print       = (int (*)(const char *, const void *, unsigned long))details_print,
	.process     = (void (*)(const char *, void *))details_process,
	.postprocess = (void (*)(void *))&details_postprocess,
	.clear       = (void (*)(void *))&details_clear,
};

static
struct stat_func scannerd = {
	.init = &scannerd_data_init,
	.fini = &free,

	.print_hdr   = scannerd_print_hdr,
	.print       = (int (*)(const char *, const void *, unsigned long))scannerd_print,
	.process     = (void (*)(const char *, void *))scannerd_process,
	.postprocess = NULL,
	.clear       = (void (*)(void *))&scannerd_clear,
};

struct stat_func * details_func = &details;
struct stat_func * scannerd_func = &scannerd;
