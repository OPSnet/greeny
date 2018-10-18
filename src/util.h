#ifndef H_GRN_UTIL
#define H_GRN_UTIL

void grn_free( void *arg );
void *grn_malloc( size_t size, int *out_err );
char *grn_strcpy_malloc( const char *in, int *out_err );

#endif
