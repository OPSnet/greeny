#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/err.h"
#include "../src/vector.h"
#include "../src/libannouncebulk.h"

static void test_sanity( void **state ) {
	;
}

static void test_vector( void **state ) {
	int in_err;
	struct vector *vec = vector_alloc( &in_err );
	assert_int_equal( in_err, GRN_OK );
	assert_int_equal( vec->allocated_n, 1 );
	assert_int_equal( vec->used_n, 0 );
	assert_non_null( vec->buffer );

	int a = 42, b = 43;
	vector_push( vec, &a, &in_err );
	assert_int_equal( in_err, GRN_OK );
	vector_push( vec, &b, &in_err );
	assert_int_equal( in_err, GRN_OK );

	assert_int_equal( vec->allocated_n, 2 );
	assert_int_equal( vec->used_n, 2 );
	assert_int_equal( *( ( int * )vec->buffer[1] ), b );

	int export_length;
	void **export_buffer = vector_export( vec, &export_length );
	assert_int_equal( export_length, 2 );
	assert_ptr_equal( export_buffer, vec->buffer );

	vector_free( vec );
}

int main( void ) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test( test_sanity ),
		cmocka_unit_test( test_vector ),
	};

	return cmocka_run_group_tests( tests, NULL, NULL );
}
