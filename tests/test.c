#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/err.h"
#include "../src/vector.h"
#include "../src/libannouncebulk.h"

#define ASSERT_OK() assert_int_equal( in_err, GRN_OK );

static void test_sanity( void **state ) {
	( void ) state;
}

static void test_vector( void **state ) {
	( void ) state;
	int in_err;

	struct vector *vec = vector_alloc( sizeof( int ), &in_err );
	ASSERT_OK();
	assert_int_equal( vec->allocated_n, 1 );
	assert_int_equal( vec->used_n, 0 );
	assert_int_equal( vec->sz, sizeof( int ) );
	assert_non_null( vec->buffer );

	int a = 42, b = 43;
	vector_push( vec, &a, &in_err );
	ASSERT_OK();
	vector_push( vec, &b, &in_err );
	ASSERT_OK();

	assert_int_equal( vec->allocated_n, 2 );
	assert_int_equal( vec->used_n, 2 );
	assert_int_equal( vector_length( vec ), 2 );
	assert_int_equal( * ( int * ) vector_get( vec, 1 ) , b );
	assert_int_equal( * ( int * ) vector_get_last( vec ) , b );
	assert_int_equal( * ( int * ) vector_pop( vec ) , b );
	assert_int_equal( vector_length( vec ), 1 );
	vector_clear( vec );
	assert_int_equal( vector_length( vec ), 0 );

	vec = vector_alloc( sizeof( int ), &in_err );
	ASSERT_OK();
	vector_push( vec, &a, &in_err );
	ASSERT_OK();
	vector_push( vec, &b, &in_err );
	ASSERT_OK();
	int export_length;
	void *pre_export_buffer = vec->buffer;
	void *export_buffer = vector_export( vec, &export_length );
	assert_int_equal( export_length, 2 );
	assert_ptr_equal( export_buffer, pre_export_buffer );
}

char *strsubst( const char *haystack, const char *find, const char *replace, int *out_err );
static void test_strsubst( void **state ) {
	( void ) state;
	int in_err;

	// no substitution
	char *hello_world = strsubst( "hello world", "the cat is in the bag", "roger", &in_err );
	ASSERT_OK();
	assert_string_equal( hello_world, "hello world" );
	free( hello_world );

	// substitution in the middle
	char *goodbye_sweet_world = strsubst( "goodbye ugly world", "ugly", "sweet", &in_err );
	ASSERT_OK();
	assert_string_equal( goodbye_sweet_world, "goodbye sweet world" );
	free( goodbye_sweet_world );

	// substitution at the beginning
	char *eat_world = strsubst( "hello world", "hello", "eat", &in_err );
	ASSERT_OK();
	assert_string_equal( eat_world, "eat world" );
	free( eat_world );

	// substitution at the end
	char *hello_mom = strsubst( "hello world", "world", "mom", &in_err );
	ASSERT_OK();
	assert_string_equal( hello_mom, "hello mom" );
	free( hello_mom );

	// multiple needles (should only replace first)
	// originally i accidentarlly replaced hello instead of world then it said "mom world"
	// and that sounds super fucking funny
	char *hello_mom_hello_mom = strsubst( "hello world. hello...mom?", "hello", "mom", &in_err );
	ASSERT_OK();
	assert_string_equal( hello_mom_hello_mom, "mom world. hello...mom?" );
	free( hello_mom_hello_mom );

	// Output is drastically longer than input
	char *hello_globglogabgolab_mom = strsubst( "hello my mom", "my", "globglogabgolab", &in_err );
	ASSERT_OK();
	assert_string_equal( hello_globglogabgolab_mom, "hello globglogabgolab mom" );
	free( hello_globglogabgolab_mom );
}

void transform_buffer( struct grn_run_ctx *ctx, int *out_err );

// line numbers aren't reported right when it's a real function. kekek
void _assert_transform_buffer_single( const char *buffer, struct grn_bencode_transform transform, char *expected_buffer ) {
	int in_err;

	transform_bencode_buffer( &my_ctx, &in_err );
	ASSERT_OK();
	assert_memory_equal( my_ctx.buffer, expected_buffer, my_ctx.buffer_n );
	free( my_ctx.buffer );
}

// test buffer transforms when they will do the transform as expected.
static void test_transform_bencode_buffer( void **state ) {
	( void ) state;
	int in_err;

	char *key_dummy[1];
	key_dummy[0] = NULL;
	char *key_presto[2];
	key_presto[0] = "presto";
	key_presto[1] = NULL;
	char *key_deep[4];
	key_deep[0] = "";
	key_deep[1] = "listo";
	key_deep[2] = "";
	key_deep[3] = NULL;

	struct grn_bencode_transform transform_set_presto = grn_mktransform_set_string( "presto", "largo" );
	transform_set_presto.key = key_dummy;
	struct grn_bencode_transform transform_del_presto = grn_mktransform_delete( "presto" );
	transform_del_presto.key = key_dummy;
	struct grn_bencode_transform transform_sub_presto = grn_mktransform_substitute( "rgo", "pd" );
	transform_sub_presto.key = key_dummy;
	struct grn_bencode_transform transform_regex_presto = grn_mktransform_substitute_regex( "m.{3}", "mm", &in_err );
	assert_int_equal( in_err, GRN_OK );
	transform_regex_presto.key = key_dummy;

	struct grn_bencode_transform transform_del_inner_presto = transform_del_presto;
	transform_del_inner_presto.key = key_presto;
	struct grn_bencode_transform transform_sub_deep = transform_sub_presto;
	transform_sub_deep.key = key_deep;

	// test when they should work normally
	_assert_transform_buffer_single( "de", transform_set_presto, "d6:presto5:largoe" );
	_assert_transform_buffer_single( "d6:presto5:largoe", transform_del_presto, "de" );
	_assert_transform_buffer_single( "5:largo", transform_sub_presto, "4:lapd" );
	_assert_transform_buffer_single( "10:imeltunity", transform_regex_presto, "8:immunity" );

	// test no-ops
	_assert_transform_buffer_single( "de", transform_del_presto, "de" );
	_assert_transform_buffer_single( "d6:presto5:largoe", transform_set_presto, "d6:presto5:largoe" );
	_assert_transform_buffer_single( "d6:presto4:lapde", transform_sub_presto, "d6:presto4:lapde" );

	// test incorrect types
	_assert_transform_buffer_single( "6:presto", transform_set_presto, "6:presto" );
	_assert_transform_buffer_single( "6:presto", transform_del_presto, "6:presto" );
	_assert_transform_buffer_single( "6:presto", transform_sub_presto, "6:presto" );

	// test invalid key
	// the substitute transform is already setup, but substitute has some special stuff to test data types so that it works with arrays. So, it is better to use del here.
	_assert_transform_buffer_single( "de", transform_del_inner_presto, "de" );

	// test recursive keys
	/**
	 * {
	 *     hello: {
	 *         what: "cd",
	 *         listo: [ "fargo", "retargo" ],
	 *     },
	 *     helloworld: "flip",
	 *     world: {
	 *         what: "cd",
	 *         listo: [ "fargo", "retargo" ],
	 *     },
	 * }
	 *
	 * into ["fapd", "rotapd"]
	 */
	_assert_transform_buffer_single(
	    "d5:hellod5:listol5:fargo7:retargoe4:what2:cde10:helloworld4:flip5:worldd5:listol5:fargo7:retargoe4:what2:cdee",
	    transform_sub_deep,
	    "d5:hellod5:listol4:fapd6:retapde4:what2:cde10:helloworld4:flip5:worldd5:listol4:fapd6:retapde4:what2:cdee"
	);
}

bool is_string_passphrase( const char * );

static void test_is_string_passphrase( void **state ) {
	( void ) state;
	assert_int_equal( is_string_passphrase( "hello" ), 0 );
	assert_int_equal( is_string_passphrase( "12345" ), 0 );
	assert_int_equal( is_string_passphrase( "abcdef1234567890abcdef1234567890" ), 1 );
	assert_int_equal( is_string_passphrase( "12345678298237846839050296378940" ), 1 );
	assert_int_equal( is_string_passphrase( "abcedecbacdcbeccadecbbeccadcecbc" ), 1 );
	assert_int_equal( is_string_passphrase( "abcdef123456789012345678901234567777" ), 2 );
}

char *normalize_announce_url( char *user_announce, char *our_announce );
int OPS_URL_LENGTH;

static void test_normalize_orpheus_announce( void **state ) {
	( void ) state;
	char our_announce[OPS_URL_LENGTH + 1];

	assert_int_equal( normalize_announce_url( "meow", our_announce ), false );
	assert_int_equal( normalize_announce_url( "https://opsfet.ch/helloworld123testing/announce", our_announce ), false );
	assert_int_equal( normalize_announce_url( "https://mars.apollo.rip/abcdef1234567890abcdef123456789/announce", our_announce ), false );
	assert_int_equal( normalize_announce_url( "https://opsfet.ch/abcdef1234567890abcdef123456789", our_announce ), false );

	assert_int_equal(
	    normalize_announce_url( "https://opsfet.ch/abcdef1234567890abcdef1234567890/announce", our_announce ),
	    true
	);
	assert_string_equal( our_announce, "https://opsfet.ch/abcdef1234567890abcdef1234567890/announce" );

	assert_int_equal(
	    normalize_announce_url( "abcdef1234567890abcdef1234567890", our_announce ),
	    true
	);
	assert_string_equal( our_announce, "https://opsfet.ch/abcdef1234567890abcdef1234567890/announce" );
}

#define RECAT_ORPHEUS_TRANSFORMS(ann) my_vec = vector_alloc(sizeof(struct grn_transform), &in_err); \
assert_int_equal(in_err, GRN_OK); \
grn_cat_transforms_orpheus(my_vec, ann, &in_err);

static void test_cat_orpheus_transforms( void **state ) {
	( void ) state;
	int in_err;
	struct vector *my_vec = vector_alloc( sizeof( struct grn_bencode_transform ), &in_err );

	RECAT_ORPHEUS_TRANSFORMS( NULL );
	assert_int_equal( in_err, GRN_ERR_ORPHEUS_ANNOUNCE_SYNTAX );
	RECAT_ORPHEUS_TRANSFORMS( "" );
	assert_int_equal( in_err, GRN_ERR_ORPHEUS_ANNOUNCE_SYNTAX );

	RECAT_ORPHEUS_TRANSFORMS( "https://flacsfor.me/abcdef0123456789abcdef0123456789/announce" );
	assert_int_equal( in_err, GRN_ERR_ORPHEUS_ANNOUNCE_SYNTAX );
	grn_free_bencode_transforms_v( my_vec );

	RECAT_ORPHEUS_TRANSFORMS( "abcdef0123456789abcdef0123456789" );
	ASSERT_OK();
	_assert_transform_buffer_single(
	    "d8:announce65:https://mars.apollo.rip/abcdef0123456789abcdef0123456789/announcee",
	    * ( struct grn_bencode_transform * ) my_vec->buffer,
	    "d8:announce59:https://opsfet.ch/abcdef0123456789abcdef0123456789/announcee"
	);
	grn_free_bencode_transforms_v( my_vec );

	RECAT_ORPHEUS_TRANSFORMS( "abcdef0123456789abcdef0123456789" );
	ASSERT_OK();
	_assert_transform_buffer_single(
	    "d8:announce60:https://xanax.rip/abcdef0123456789abcdef0123456789/announce/e",
	    * ( struct grn_bencode_transform * ) my_vec->buffer,
	    "d8:announce59:https://opsfet.ch/abcdef0123456789abcdef0123456789/announcee"
	);
	grn_free_bencode_transforms_v( my_vec );

	RECAT_ORPHEUS_TRANSFORMS( "abcdef0123456789abcdef0123456789" );
	ASSERT_OK();
	_assert_transform_buffer_single(
	    "d8:announce65:https://maps.apollo.rip/abcdef0123456789abcdef0123456789/announcee",
	    * ( struct grn_bencode_transform * ) my_vec->buffer,
	    "d8:announce65:https://maps.apollo.rip/abcdef0123456789abcdef0123456789/announcee"
	);
	grn_free_bencode_transforms_v( my_vec );
}

static void test_usrcs_to_srcs( void **state ) {
	( void ) state;
	int in_err;

	struct grn_source *srcs;
	int srcs_n;

	struct grn_user_source torrent_usrc = {
		.type = GRN_USRC_TORRENT,
		.path = "/bin/sh",
	};
	puts( "GRN_USRC_TORRENT" );
	srcs = grn_usrcs_to_srcs( &torrent_usrc, 1, &srcs_n, &in_err );
	ASSERT_OK();
	assert_int_equal( srcs_n, 1 );
	assert_int_equal( srcs[0].type, GRN_SRC_TORRENT );
	assert_string_equal( srcs[0].path, torrent_usrc.path );
	grn_free_srcs(srcs, srcs_n);

	struct grn_user_source recursive_usrc = {
		.type = GRN_USRC_RECURSIVE_TORRENT,
		.path = "tests/fixtures/recursive",
	};
	puts( "GRN_USRC_RECURSIVE_TORRENT" );
	srcs = grn_usrcs_to_srcs( &recursive_usrc, 1, &srcs_n, &in_err );
	ASSERT_OK();
	assert_int_equal( srcs_n, 1 );
	assert_int_equal( srcs[0].type, GRN_SRC_TORRENT );
	assert_string_equal( srcs[0].path, "tests/fixtures/recursive/empty.torrent" );
	grn_free_srcs(srcs, srcs_n);

	// TODO: transmission. Since transmission is just a recursive with a default directory, it's a bit weird

	struct grn_user_source qbittorrent_usrc = {
		.type = GRN_USRC_QBITTORRENT,
		.path = "tests/fixtures/qbittorrent",
	};
	puts( "GRN_USRC_QBITTORRENT" );
	srcs = grn_usrcs_to_srcs( &qbittorrent_usrc, 1, &srcs_n, &in_err );
	ASSERT_OK();
	assert_int_equal( srcs_n, 2 );
	assert_int_equal( srcs[0].type, GRN_SRC_TORRENT );
	assert_string_equal( srcs[0].path, "tests/fixtures/qbittorrent/BT_Backup/empty.torrent" );
	assert_int_equal( srcs[1].type, GRN_SRC_QBITTORRENT_FASTRESUME );
	assert_string_equal( srcs[1].path, "tests/fixtures/qbittorent/BT_Backup/empty.fastresume" );
	grn_free_srcs(srcs, srcs_n);

	struct grn_user_source deluge_usrc = {
		.type = GRN_USRC_DELUGE,
		.path = "tests/fixtures/deluge",
	};
	puts( "GRN_USRC_DELUGE" );
	srcs = grn_usrcs_to_srcs( &deluge_usrc, 1, &srcs_n, &in_err );
	ASSERT_OK();
	assert_int_equal( srcs_n, 2 );
	assert_int_equal( srcs[0].type, GRN_SRC_TORRENT );
	assert_string_equal( srcs[0].path, "tests/fixtures/deluge/state/empty.torrent" );
	assert_int_equal( srcs[1].type, GRN_SRC_DELUGE_STATE );
	assert_string_equal( srcs[1].path, "tests/fixtures/deluge/state/torrents.state" );
	grn_free_srcs(srcs, srcs_n);

	struct grn_user_source utorrent_usrc = {
		.type = GRN_USRC_UTORRENT,
		.path = "tests/fixtures/utorrent",
	};
	puts( "GRN_USRC_TORRENT" );
	srcs = grn_usrcs_to_srcs( &utorrent_usrc, 1, &srcs_n, &in_err );
	ASSERT_OK();
	assert_int_equal( srcs_n, 1 );
	assert_int_equal( srcs[0].type, GRN_SRC_TORRENT );
	assert_string_equal( srcs[0].path, "tests/fixtures/utorrent/empty.torrent" );
	assert_int_equal( srcs[1].type, GRN_SRC_UTORRENT_RESUME );
	grn_free_srcs(srcs, srcs_n);
}

static void test_utrans_to_bencode_transforms(void **state) {
	(void) state;
	int in_err;

	struct grn_bencode_transform *btrans;
	int btrans_n;

	struct grn_utrans_announce_substitute subst = {
		.type = GRN_UTRANS_ANNOUNCE_SUBSTITUTE,
		.find = "find",
		.replace = "replace",
	};

	struct grn_bencode_transform subst_key = {

	};
	grn_utrans_to_bencode_transforms((struct grn_utrans *) &subst, GRN_SRC_TORRENT, &btrans_n, &in_err);
	ASSERT_OK();


}

int main( void ) {
	const struct cmunittest tests[] = {
		cmocka_unit_test( test_sanity ),
		cmocka_unit_test( test_vector ),
		cmocka_unit_test( test_strsubst ),
		cmocka_unit_test( test_transform_bencode_buffer ),
		cmocka_unit_test( test_is_string_passphrase ),
		cmocka_unit_test( test_normalize_orpheus_announce ),
		cmocka_unit_test( test_cat_orpheus_transforms ),
		cmocka_unit_test( test_usrcs_to_srcs ),
		cmocka_unit_test( test_utrans_to_bencode_transforms ),
	};

	return cmocka_run_group_tests( tests, NULL, NULL );
}



