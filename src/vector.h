#ifndef H_VECTOR
#define H_VECTOR

struct vector {
	void **buffer;
	int used_n;
	int allocated_n;
};

struct vector *vector_alloc(int *out_err);
void vector_free(struct vector *free_me);
void vector_push(struct vector *vector, void *push_me, int *out_err);
size_t vector_length(const struct vector *vector);
/**
 * Convert a vector into a simple buffer. No need to run vector_free afterwards.
 * @param vector the vector to export
 * @param n where to put the size of the exported vector.
 * @return pointer to the dynamically allocated array of vector contents.
 */
void **vector_export(struct vector *vector, int *n, int *out_err);

#endif
