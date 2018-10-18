#ifndef H_VECTOR
#define H_VECTOR

struct vector {
	void *buffer;
	int used_n;
	int allocated_n;
	int sz;
};

struct vector *vector_alloc( int sz, int *out_err );
// noop if null
void vector_free( struct vector *free_me );
// frees children. Only use if they are pointers!
void vector_free_all( struct vector *free_me );
// will copy push_me by value
void vector_push( struct vector *vector, void *push_me, int *out_err );
size_t vector_length( const struct vector *vector );
void *vector_get( struct vector *vector, int i );
void *vector_get_last( struct vector *vector );
// the element returned is only valid until the vector is modified.
void *vector_pop( struct vector *vector );
// do not access previously popped or exported after this
void vector_clear( struct vector *vector );
/**
 * Convert a vector into a simple buffer. You still must free the vector later.
 * @param vector the vector to export
 * @param n where to put the size of the exported vector.
 * @return pointer to the dynamically allocated array of vector contents.
 */
void *vector_export( struct vector *vector, int *n );

#endif
