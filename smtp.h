/* SPDX-License-Identifier: GPL-3.0-or-later */

extern struct stat_func * smtp_func;

void ratelimitspp_clear();
int  ratelimitspp_print_hdr();
int  ratelimitspp_print(const unsigned long time);
