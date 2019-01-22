struct stat_func {
	void * (*init)       ();
	void (*fini)         (void *);
	void (*print_hdr)    (const char *);
	void (*clear)        (void *);
	void (*print)        (const char *, const void *, unsigned long);
	void (*process)      (const char *, void *);
	void (*postprocess)  (void *);
};
