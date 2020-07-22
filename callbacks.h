/* SPDX-License-Identifier: GPL-3.0-or-later */

struct stat_func {
	void * (*init)       ();
	void (*fini)         (void *);
	int  (*print_hdr)    (const char *);
	void (*clear)        (void *);
	int  (*print)        (const char *, const void *, unsigned long);
	void (*process)      (const char *, void *);
	void (*postprocess)  (void *);
};
