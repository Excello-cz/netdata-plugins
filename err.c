#include "err.h"

char *
nd_err_to_str(const enum nd_err err) {
	switch (err) {
	case ND_SUCCUESS: return "Success";
	case ND_ERROR: return "General error";
	case ND_INOTIFY: return "Inotify error";
	case ND_ALLOC: return "Allocation error";
	case ND_FILE: return "File error";
	default:
		return "Unknown";
	}
}
