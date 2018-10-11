#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "vector.h"
#include "err.h"

struct vector *vector_alloc( int sz, int *out_err ) {
	*out_err = GRN_OK;

	struct vector *to_return = malloc( sizeof( struct vector ) );
	if ( to_return == NULL ) {
		*out_err = GRN_ERR_OOM;
		return NULL;
	}
	to_return->buffer = malloc( sz );
	if ( to_return->buffer == NULL ) {
		free( to_return );
		*out_err = GRN_ERR_OOM;
		return NULL;
	}
	to_return->used_n = 0;
	to_return->allocated_n = 1;
	to_return->sz = sz;
	return to_return;
}

void vector_free( struct vector *free_me ) {
	assert( free_me != NULL );
	assert( free_me->buffer != NULL );
	free( free_me->buffer );
	free( free_me );
}

void vector_push( struct vector *vector, void *push_me, int *out_err ) {
	assert( vector != NULL );
	assert( push_me != NULL );
	vector->used_n++;
	if ( vector->used_n > vector->allocated_n ) {
		vector->allocated_n *= 2;
		void **buffer_new = realloc( vector->buffer, vector->allocated_n * vector->sz );
		ERR( vector->buffer == NULL, GRN_ERR_OOM );
		vector->buffer = buffer_new;
	}
	memcpy( vector->buffer + ( vector->used_n - 1 ) * vector->sz , push_me, vector->sz );
	RETURN_OK();
}

size_t vector_length( const struct vector *vector ) {
	return vector->used_n;
}

void *vector_export( struct vector *vector, int *n ) {
	*n = vector->used_n;
	void *to_return = vector->buffer;
	free( vector );
	return to_return;
}
