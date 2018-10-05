#ifndef H_VECTOR
#define H_VECTOR

struct vector {
	void **buffer;
	int used_n;
	int allocated_n;
};

struct vector *vector_alloc( int *out_err );
void vector_free( struct vector *free_me );
void vector_push( struct vector *vector, void *push_me, int *out_err );
/**
 * @param vector the vector to push onto
 * @param push_me an array of multiple elements to push
 * @param push_me_n the length of push_me. If -1, push_me should be NULL-terminated
 */
void vector_push_all( struct vector *vector, void **push_me, int push_me_n, int *out_err );
size_t vector_length( const struct vector *vector );
/**
 * Convert a vector into a simple buffer. You still must free the vector later.
 * @param vector the vector to export
 * @param n where to put the size of the exported vector.
 * @return pointer to the dynamically allocated array of vector contents.
 */
void **vector_export( struct vector *vector, int *n );
/**
 * Frees all vector elements, followed by the vector itself.
 * @param vector vector to free children of.
 */
void vector_free_all( struct vector *free_me );

/**
 * Moves values into contiguous memory
 * @param ptrs pointer to the first element in an array of pointers
 * @param ptrs_n the number of pointers in said array
 * @param sz the size of each pointed-to member
 * @return dynamically allocated pointer to the flattened
 */
// fuck size_t
void *grn_flatten_ptrs( void **ptrs, int ptrs_n, int sz, int *out_err );

#endif
