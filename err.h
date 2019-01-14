enum nd_err {
	ND_SUCCUESS,
	ND_ERROR,

	ND_ALLOC,
	ND_INOTIFY,
};

char *
nd_err_to_str(const enum nd_err);
