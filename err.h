enum nd_err {
	ND_SUCCESS,
	ND_ERROR,

	ND_ALLOC,
	ND_INOTIFY,
	ND_FILE,
};

char *
nd_err_to_str(const enum nd_err);
