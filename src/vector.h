#ifndef H_VECTOR
#define H_VECTOR

struct vector {
	void **buffer;
	int used_n;
	int allocated_n;
};

struct vector *vector_alloc(int *out_err);
void vector_free(struct vector *free_me);
void vector_push(void *push_me, struct vector *vector, int *out_err);
void **vector_export(struct vector *vector, int *n, int *out_err);

#endif
