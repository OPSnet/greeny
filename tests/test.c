#include <stdlib.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/err.h"
#include "../src/vector.h"
#include "../src/libannouncebulk.h"

static void test_sanity( void **state ) {
	( void ) state;
}

static void test_vector( void **state ) {
	( void ) state;
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

char *strsubst( const char *haystack, const char *find, const char *replace, int *out_err );
static void test_strsubst( void **state ) {
	( void ) state;
	int in_err;

	// no substitution
	char *hello_world = strsubst( "hello world", "the cat is in the bag", "roger", &in_err );
	assert_int_equal( in_err, GRN_OK );
	assert_string_equal( hello_world, "hello world" );
	free( hello_world );

	// substitution in the middle
	char *goodbye_sweet_world = strsubst( "goodbye ugly world", "ugly", "sweet", &in_err );
	assert_int_equal( in_err, GRN_OK );
	assert_string_equal( goodbye_sweet_world, "goodbye sweet world" );
	free( goodbye_sweet_world );

	// substitution at the beginning
	char *eat_world = strsubst( "hello world", "hello", "eat", &in_err );
	assert_int_equal( in_err, GRN_OK );
	assert_string_equal( eat_world, "eat world" );
	free( eat_world );

	// substitution at the end
	char *hello_mom = strsubst( "hello world", "world", "mom", &in_err );
	assert_int_equal( in_err, GRN_OK );
	assert_string_equal( hello_mom, "hello mom" );
	free( hello_mom );

	// multiple needles (should only replace first)
	// originally i accidentarlly replaced hello instead of world then it said "mom world"
	// and that sounds super fucking funny
	char *hello_mom_hello_mom = strsubst( "hello world. hello...mom?", "hello", "mom", &in_err );
	assert_int_equal( in_err, GRN_OK );
	assert_string_equal( hello_mom_hello_mom, "mom world. hello...mom?" );
	free( hello_mom_hello_mom );

	// Output is drastically longer than input
	char *hello_globglogabgolab_mom = strsubst( "hello my mom", "my", "globglogabgolab", &in_err );
	assert_int_equal( in_err, GRN_OK );
	assert_string_equal( hello_globglogabgolab_mom, "hello globglogabgolab mom" );
	free( hello_globglogabgolab_mom );
}

// test buffer transforms when they will do the transform as expected.
static void test_transform_buffer_ok( void **state ) {
	( void ) state;
	int in_err;
}

int main( void ) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test( test_sanity ),
		cmocka_unit_test( test_vector ),
		cmocka_unit_test( test_strsubst ),
		cmocka_unit_test( test_transform_buffer_ok ),
	};

	return cmocka_run_group_tests( tests, NULL, NULL );
}
