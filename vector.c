#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "vector.h"

enum nd_err
vector_init(struct vector * v, const size_t s) {
	static const size_t INITIAL_CAPACITY = 64;
	if (!(v->data = malloc(s * INITIAL_CAPACITY))) {
		return ND_ALLOC;
	}
	v->cap = INITIAL_CAPACITY;
	v->size = s;
	v->len = 0;
	return ND_SUCCESS;
}

static
enum nd_err
vector_resize(struct vector * v) {
	if (v->len == v->cap) {
		const size_t newcap = v->cap * 3 / 2;
		void * newdata = realloc(v->data, newcap * v->size);
		if (newdata) {
			v->data = newdata;
			v->cap = newcap;
		} else {
			return ND_ALLOC;
		}
	}
	return ND_SUCCESS;
}

enum nd_err
vector_add(struct vector * v, const void * new) {
	enum nd_err ret;
	if ((ret = vector_resize(v)) == ND_SUCCESS) {
		void * item = vector_item(v, v->len);
		memcpy(item, new, v->size);
		v->len++;
	}
	return ret;
}

int
vector_search(struct vector * v, const void * item) {
	int i;
	for (i = 0; i < v->len; i++)
		if (!memcmp(item, vector_item(v, i), v->size))
			return i;

	return -1;
}

void
vector_free(struct vector * v) {
	free(v->data);
	v->data = NULL;
}
