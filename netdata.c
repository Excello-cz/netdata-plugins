/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

#include "netdata.h"

static
const char *
nd_algorithm_str[] = {
	"absolute",
	"incremental",
	"percentage-of-absolute-row",
	"percentage-of-incremental-row",
};

static
const char *
nd_charttype_str[] = {
	"line",
	"area",
	"stacked",
};

static
const char *
check_null(const char * str) {
	return str ? str : "";
}

void
nd_chart(const char * type, const char * prefix, const char * id, const char * name,
		const char * title, const char * units, const char * family, const char * context,
		enum nd_charttype chart_type) {
	printf("\nCHART %s.%s_%s '%s' '%s' '%s' '%s' '%s' %s\n", type, check_null(prefix), id,
		check_null(name), check_null(title), check_null(units), check_null(family),
		check_null(context), nd_charttype_str[chart_type]);
}

void
nd_disable() {
	puts("DISABLE");
}

void
nd_dimension(const char * id, const char * name, enum nd_algorithm alg,
		int multiplier, int divisor, enum nd_visibility visibility) {
	printf("DIMENSION %s '%s' %s %d %d",
		id, check_null(name), nd_algorithm_str[alg], multiplier, divisor);
	if (visibility == ND_HIDDEN) {
		fputs(" hidden", stdout);
	}
	putchar('\n');
}

void
nd_begin(const char * type, const char * prefix, const char * id) {
	nd_begin_time(type, prefix, id, 0);
}

void
nd_begin_time(const char * type, const char * prefix, const char * id, const unsigned long time) {
	printf("\nBEGIN %s.%s_%s", type, check_null(prefix), id);
	if (time)
		printf(" %lu", time);
	putchar('\n');
}

void
nd_end() {
	puts("END");
}

void
nd_set(const char * name, const long value) {
	printf("SET %s = %ld\n", name, value);
}
