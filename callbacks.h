struct stat_func {
	void * (*init)       ();
	void (*fini)         (void *);
	void (*print_hdr)    ();
	void (*clear)        (void *);
	void (*print)        (const void *);
	void (*process)      (const char *, void *);
	void (*postprocess)  (void *);
};
