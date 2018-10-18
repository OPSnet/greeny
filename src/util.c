#include <stdlib.h>
#include <string.h>

#include "err.h"

void grn_free( void *arg ) {
	if ( arg != NULL ) {
		free( arg );
	}
}


void *grn_malloc( size_t sz, int *out_err ) {
	*out_err = GRN_OK;

	void *to_return = malloc( sz );
	ERR_NULL( to_return == NULL, GRN_ERR_OOM );
	return to_return;
}

char *grn_strcpy_malloc( const char *in, int *out_err ) {
	*out_err = GRN_OK;

	char *to_return = grn_malloc( strlen( in ) + 1, out_err );
	ERR_FW_NULL();
	strcpy( to_return, in );
	return to_return;
}
