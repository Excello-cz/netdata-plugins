struct statistics {
	int tcp_ok;
	int tcp_deny;
	int tcp_status;
	int tcp_status_sum;
	int tcp_status_count;

	int tcp_end_status_0;
	int tcp_end_status_256;
	int tcp_end_status_25600;
	int tcp_end_status_others;
};

void print_smtp_header();
void print_smtp_data();
void process_smtp(const char *, struct statistics *);
