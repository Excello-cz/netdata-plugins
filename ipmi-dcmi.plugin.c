#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <freeipmi/freeipmi.h>
#include <freeipmi/api/ipmi-dcmi-cmds-api.h>

#include "err.h"
#include "netdata.h"
#include "timer.h"

struct ipmi_dcmi_stat {
	uint16_t current_power;
	uint16_t minimum_power_over_sampling_duration;
	uint16_t maximum_power_over_sampling_duration;
	uint16_t average_power_over_sampling_duration;
#ifdef IPMI_DCMI_POWER_READ_ALL
	uint32_t time_stamp;
	uint32_t statistics_reporting_time_period;
	uint8_t power_measurement;
#endif
};

static
fiid_obj_t obj_cmd_rs = NULL;

static
int run;

static
void
quit(int unused) {
	run = 0;
}

static int
read_data(ipmi_ctx_t ipmi_ctx, struct ipmi_dcmi_stat * data) {
	uint64_t val;

	if (fiid_obj_clear(obj_cmd_rs)) {
		fprintf(stderr, "ipmi-dcmi.plugin: fiid_obj_clear: %s\n", fiid_obj_errormsg(obj_cmd_rs));
		return ND_ERROR;
	}
	if (ipmi_cmd_dcmi_get_power_reading(ipmi_ctx, IPMI_DCMI_POWER_READING_MODE_SYSTEM_POWER_STATISTICS, 0, obj_cmd_rs) < 0) {
		fprintf(stderr, "ipmi-dcmi.plugin: ipmi_cmd_dcmi_get_power_reading: %s\n", ipmi_ctx_errormsg(ipmi_ctx));
		return ND_ERROR;
	}

	if (FIID_OBJ_GET(obj_cmd_rs, "current_power", &val) < 0) {
		fprintf(stderr, "ipmi-dcmi.plugin: fiid_obj_get: 'current_power': %s\n", fiid_obj_errormsg(obj_cmd_rs));
		return ND_ERROR;
	}
	data->current_power = val;

	if (FIID_OBJ_GET(obj_cmd_rs, "minimum_power_over_sampling_duration", &val) < 0) {
		fprintf(stderr, "ipmi-dcmi.plugin: fiid_obj_get: 'minimum_power_over_sampling_duration': %s\n",
			fiid_obj_errormsg(obj_cmd_rs));
		return ND_ERROR;
	}
	data->minimum_power_over_sampling_duration = val;

	if (FIID_OBJ_GET(obj_cmd_rs, "maximum_power_over_sampling_duration", &val) < 0) {
		fprintf(stderr, "ipmi-dcmi.plugin: fiid_obj_get: 'maximum_power_over_sampling_duration': %s\n",
			fiid_obj_errormsg(obj_cmd_rs));
		return ND_ERROR;
	}
	data->maximum_power_over_sampling_duration = val;

	if (FIID_OBJ_GET(obj_cmd_rs, "average_power_over_sampling_duration", &val) < 0) {
		fprintf(stderr, "ipmi-dcmi.plugin: fiid_obj_get: 'average_power_over_sampling_duration': %s\n",
			fiid_obj_errormsg(obj_cmd_rs));
		return ND_ERROR;
	}
	data->average_power_over_sampling_duration = val;

#ifdef IPMI_DCMI_POWER_READ_ALL
	if (FIID_OBJ_GET(obj_cmd_rs, "time_stamp", &val) < 0) {
		fprintf(stderr, "ipmi-dcmi.plugin: fiid_obj_get: 'time_stamp': %s\n", fiid_obj_errormsg(obj_cmd_rs));
		return ND_ERROR;
	}
	data->time_stamp = val;

	if (FIID_OBJ_GET(obj_cmd_rs, "statistics_reporting_time_period", &val) < 0) {
		fprintf(stderr, "ipmi-dcmi.plugin: fiid_obj_get: 'statistics_reporting_time_period': %s\n",
			fiid_obj_errormsg(obj_cmd_rs));
		return ND_ERROR;
	}
	data->statistics_reporting_time_period = val;

	if (FIID_OBJ_GET(obj_cmd_rs, "power_reading_state.power_measurement", &val) < 0) {
		fprintf(stderr, "ipmi-dcmi.plugin: fiid_obj_get: 'power_measurement': %s\n", fiid_obj_errormsg(obj_cmd_rs));
		return ND_ERROR;
	}
	data->power_measurement = val;
#endif

	return ND_SUCCESS;
}

int
main(int argc, char **argv) {
	int ret = 0;
	int timeout = 1;
	struct timespec timestamp;
	struct ipmi_dcmi_stat data = {0};

	/* skip argv0 */
	argv++; argc--;

	if (argc == 1) {
		timeout = atoi(*argv);
	} else if (argc > 1) {
		fprintf(stderr, "usage: ipmi-dcmi.plugin [timeout]\n");
		return 1;
	}

	if (timeout < 1) {
		fprintf(stderr, "ipmi-dcmi.plugin: timeout must be positive number, got: %s\n", *argv);
		return 1;
	}

	signal(SIGQUIT, quit);
	signal(SIGTERM, quit);
	signal(SIGINT, quit);

	ipmi_ctx_t ipmi_ctx = ipmi_ctx_create();
	if (!ipmi_ctx) {
		perror("ipmi-dcmi.plugin: ipmi_ctx_create()");
		return 0;
	}

	switch (ipmi_ctx_find_inband(
		ipmi_ctx,
		NULL, /* driver type */
		0, /* disable auto probe */
		0, /* driver address */
		0, /* register spacing */
		NULL, /* driver device */
		0, /* workaround flags */
		IPMI_FLAGS_DEFAULT
	)) {
	case 1:
		break;
	case 0:
		fprintf(stderr, "ipmi-dcmi.plugin: ipmi_ctx_find_inband: driver not found\n");
		ret = 1;
		goto cleanup;
	default:
		fprintf(stderr, "ipmi-dcmi.plugin: ipmi_ctx_find_inband: %s\n", ipmi_ctx_errormsg(ipmi_ctx));
		goto cleanup;
	}

	obj_cmd_rs = fiid_obj_create(tmpl_cmd_dcmi_get_power_reading_rs);
	if (obj_cmd_rs == NULL) {
		fprintf(stderr, "ipmi-dcmi.plugin: fiid_obj_create: %s\n", strerror(errno));
		goto cleanup;
	}

	nd_chart("ipmi", "dcmi_power", NULL, NULL, "IPMI DCMI Power",
		"Watts", "power", "ipmi.dcmi", ND_CHART_TYPE_LINE);
	nd_dimension("current", "current", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("minimum", "minimum", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("maximum", "maximum", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
	nd_dimension("average", "average", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

#ifdef IPMI_DCMI_POWER_READ_ALL
	nd_chart("ipmi", "dcmi_timestamp", NULL, NULL, "IPMI DCMI timestamp",
		"Seconds", "time", "ipmi.dcmi", ND_CHART_TYPE_LINE);
	nd_dimension("timestamp", "timestamp", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);

	nd_chart("ipmi", "dcmi_stat_period", NULL, NULL, "IPMI DCMI Statistics reporting time period",
		"Seconds", "period", "ipmi.dcmi", ND_CHART_TYPE_LINE);
	nd_dimension("period", "period", ND_ALG_ABSOLUTE, 1, 1000, ND_VISIBLE);

	nd_chart("ipmi", "dcmi_measurement", NULL, NULL, "IPMI DCMI Measurement",
		"On/Off", "measurement", "ipmi.dcmi", ND_CHART_TYPE_LINE);
	nd_dimension("state", "state", ND_ALG_ABSOLUTE, 1, 1, ND_VISIBLE);
#endif

	clock_gettime(CLOCK_REALTIME, &timestamp);

	for (run = 1; run;) {
		unsigned long last_update = update_timestamp(&timestamp);

		read_data(ipmi_ctx, &data);

		nd_begin_time("ipmi", "dcmi_power", NULL, last_update);
		nd_set("current", data.current_power);
		nd_set("minimum", data.minimum_power_over_sampling_duration);
		nd_set("maximum", data.maximum_power_over_sampling_duration);
		nd_set("average", data.average_power_over_sampling_duration);
		nd_end();

#ifdef IPMI_DCMI_POWER_READ_ALL
		nd_begin_time("ipmi", "dcmi_timestamp", NULL, last_update);
		nd_set("timestamp", data.time_stamp);
		nd_end();

		nd_begin_time("ipmi", "dcmi_stat_period", NULL, last_update);
		nd_set("period", data.statistics_reporting_time_period);
		nd_end();

		nd_begin_time("ipmi", "dcmi_measurement", NULL, last_update);
		nd_set("state", data.power_measurement);
		nd_end();
#endif

		if (fflush(stdout) == EOF) {
			fprintf(stderr, "ipmi-dcmi.plugin: cannot write to stdout: %s\n", strerror(errno));
			break;
		}

		sleep(timeout);
	}

cleanup:
	ipmi_ctx_close(ipmi_ctx);
	ipmi_ctx_destroy(ipmi_ctx);

	return ret;
}
