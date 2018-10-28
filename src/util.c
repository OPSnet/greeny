#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

// from https://stackoverflow.com/a/14530993/1233320
void grn_decode_url( char *dst, const char *src ) {
	char a, b;
	while ( *src ) {
		if ( ( *src == '%' ) &&
		        ( ( a = src[1] ) && ( b = src[2] ) ) &&
		        ( isxdigit( a ) && isxdigit( b ) ) ) {
			if ( a >= 'a' )
				a -= 'a' - 'A';
			if ( a >= 'A' )
				a -= ( 'A' - 10 );
			else
				a -= '0';
			if ( b >= 'a' )
				b -= 'a' - 'A';
			if ( b >= 'A' )
				b -= ( 'A' - 10 );
			else
				b -= '0';
			*dst++ = 16 * a + b;
			src += 3;
		} else if ( *src == '+' ) {
			*dst++ = ' ';
			src++;
		} else {
			*dst++ = *src++;
		}
	}
	*dst++ = '\0';
}
