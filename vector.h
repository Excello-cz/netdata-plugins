struct vector {
	size_t cap;  /* vector capapcity */
	size_t size; /* size of one item */
	size_t len;  /* number of items in vector */
	void * data; /* pointer to data */
};

#define VECTOR_EMPTY { .cap = 0, .size = 0, .len = 0, .data = NULL }

static inline
int
vector_is_init(struct vector * v) {
	return v && v->data;
}

static inline
int
vector_is_empty(struct vector * v) {
	return !vector_is_init(v) || v->len == 0;
}

enum nd_err
vector_init(struct vector *, const size_t);

enum nd_err
vector_add(struct vector *, const void *);

static inline
void *
vector_item(struct vector * v, const size_t idx) {
	return (char *)v->data + idx * v->size;
}

int
vector_search(struct vector *, const void *);

void
vector_free(struct vector *);
