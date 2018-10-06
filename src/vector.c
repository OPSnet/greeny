#include <stdlib.h>
#include <string.h>

#include "vector.h"
#include "err.h"

struct vector *vector_alloc( int *out_err ) {
	*out_err = GRN_OK;

	struct vector *to_return = malloc( sizeof( struct vector ) );
	if ( to_return == NULL ) {
		*out_err = GRN_ERR_OOM;
		return NULL;
	}
	to_return->buffer = malloc( sizeof( void * ) );
	if ( to_return->buffer == NULL ) {
		free( to_return );
		*out_err = GRN_ERR_OOM;
		return NULL;
	}
	to_return->used_n = 0;
	to_return->allocated_n = 1;
	return to_return;
}

void vector_free( struct vector *free_me ) {
	free( free_me->buffer );
	free( free_me );
}

void vector_free_all( struct vector *free_me ) {
	for ( int i = 0; i < vector_length( free_me ); i++ ) {
		free( free_me->buffer[i] );
	}
	vector_free( free_me );
}

void vector_push( struct vector *vector, void *push_me, int *out_err ) {
	vector->used_n++;
	if ( vector->used_n > vector->allocated_n ) {
		vector->allocated_n *= 2;
		void **buffer_new = realloc( vector->buffer, vector->allocated_n * sizeof( void * ) );
		ERR( !vector->buffer, GRN_ERR_OOM );
		vector->buffer = buffer_new;
	}
	vector->buffer[vector->used_n - 1] = push_me;
	RETURN_OK();
}

void vector_push_all( struct vector *vector, void **push_me, int push_me_n, int *out_err ) {
	*out_err = GRN_OK;

	int push_me_c = 0;
	// break
	while ( push_me_n > -1 ? push_me_c < push_me_n :  * ( push_me + push_me_c ) != NULL ) {
		vector_push( vector, push_me + push_me_c, out_err );
		ERR_FW();
	}
}

size_t vector_length( const struct vector *vector ) {
	return vector->used_n;
}

void **vector_export( struct vector *vector, int *n ) {
	*n = vector->used_n;
	void **to_return = vector->buffer;
	return to_return;
}

void *grn_flatten_ptrs( void **ptrs, int ptrs_n, int sz, int *out_err ) {
	*out_err = GRN_OK;

	void *to_return = malloc( ptrs_n * sz );
	ERR_NULL( to_return == NULL, GRN_ERR_OOM );
	for ( int i = 0; i < ptrs_n; i++ ) {
		void *to_return_with_off = ( void * )( ( ( char * )to_return ) + sz * i );
		memcpy( to_return_with_off, ptrs[i], sz );
	}
	return to_return;
}
