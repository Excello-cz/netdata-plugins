/* SPDX-License-Identifier: GPL-3.0-or-later */

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

	int ratelimitspp_conn_timeout;
	int ratelimitspp_error;
	int ratelimitspp_ratelimited;
};

extern struct stat_func * smtp_func;
