#include <stdlib.h>

#include "vector.h"
#include "err.h"

struct vector *vector_alloc(int *out_err) {
	struct vector *to_return;
	to_return = malloc(sizeof(struct vector *));
	ERR_NULL(!to_return, GRN_ERR_OOM);
	to_return->buffer = malloc(sizeof(void *));
	ERR_NULL(!to_return->buffer, GRN_ERR_OOM);
	to_return->used_n = 0;
	to_return->allocated_n = 1;
	RETURN_OK(to_return);
}

void vector_free(struct vector *free_me) {
	free(free_me->buffer);
	free(free_me);
}

void vector_push(struct vector *vector, void *push_me, int *out_err) {
	vector->used_n++;
	if (vector->used_n > vector->allocated_n) {
		vector->allocated_n *= 2;
		vector->buffer = realloc(vector->buffer, vector->allocated_n * sizeof(void*));
		ERR(!vector->buffer, GRN_ERR_OOM);
	}
	vector->buffer[vector->used_n - 1] = push_me;
	RETURN_OK();
}

size_t vector_length(const struct vector *vector) {
	return vector->used_n;
};

void **vector_export(struct vector *vector, int *n, int *out_err) {
	*n = vector->used_n;
	void **to_return;
	to_return = realloc(vector->buffer, vector->used_n * sizeof(void*));
	ERR_NULL(!to_return, GRN_ERR_OOM);
	free(vector);
	RETURN_OK(to_return);
}
