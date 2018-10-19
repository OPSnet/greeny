#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ftw.h>
#include <ctype.h>
#include <regex.h>

#include <bencode.h>

#include "libannouncebulk.h"
#include "vector.h"
#include "util.h"
#include "err.h"

// BEGIN context filesystem

void fread_ctx( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;
	assert( ctx->state == GRN_CTX_READ );
	assert( ctx->fh != NULL );

	// we can't just do fread(buffer, 1, some_massive_num, fh) because we can't be sure whether
	// the whole file was read or not.
	ERR( fseek( ctx->fh, 0, SEEK_END ), GRN_ERR_FS_SEEK );
	ctx->buffer_n = ftell( ctx->fh );
	GRN_LOG_DEBUG( "File size: %d bytes", ( int )ctx->buffer_n );
	ERR( fseek( ctx->fh, 0, SEEK_SET ), GRN_ERR_FS_SEEK );

	ctx->buffer = malloc( ctx->buffer_n + 1 );
	ERR( ctx->buffer == NULL, GRN_ERR_OOM );
	if ( fread( ctx->buffer, ctx->buffer_n, 1, ctx->fh ) != 1 ) {
		free( ctx->buffer );
		ctx->buffer = NULL;
		ERR( GRN_ERR_FS_READ );
	}
}

void fwrite_ctx( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;
	assert( ctx->state == GRN_CTX_WRITE );
	assert( ctx->fh != NULL );

	ERR( fwrite( ctx->buffer, ctx->buffer_n, 1, ctx->fh ) != 1, GRN_ERR_FS_WRITE );
}

// END context filesystem

// BEGIN custom data type operations

struct grn_ctx *grn_ctx_alloc( int *out_err ) {
	*out_err = GRN_OK;

	struct grn_ctx *ctx = calloc( 1, sizeof( struct grn_ctx ) );
	ERR_NULL( ctx == NULL, GRN_ERR_OOM );

	ctx->state = GRN_CTX_NEXT;
	ctx->files_c = -1;
	return ctx;
}

void grn_ctx_free( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;

	if ( ctx == NULL ) {
		return;
	}
	if ( ctx->files != NULL ) {
		for ( int i = 0; i < ctx->files_n; i++ ) {
			if ( ctx->files[i] == NULL ) {
				break;
			}
			free( ctx->files[i] );
		}
		free( ctx->files );
	}

	if ( ctx->transforms != NULL ) {
		for ( int i = 0; i < ctx->transforms_n; i++ ) {
			grn_free_transform( ctx->transforms + i );
		}
		free( ctx->transforms );
	}
	grn_free( ctx->buffer );
	if ( ctx->fh != NULL ) {
		// we still want to continue when the fclose fails, to free the ctx
		if ( fclose( ctx->fh ) ) {
			*out_err = GRN_ERR_FS_CLOSE;
		}
	}
	free( ctx );
}

// maybe I should stop pretending C is object oriented? But the ctx is supposed to be opaque, right?
void grn_ctx_set_files( struct grn_ctx *ctx, char **files, int files_n ) {
	ctx->files = files;
	ctx->files_n = files_n;
}

void grn_ctx_set_files_v( struct grn_ctx *ctx, struct vector *files ) {
	ctx->files = ( char ** ) vector_export( files, &ctx->files_n );
}

void grn_ctx_set_transforms( struct grn_ctx *ctx, struct grn_transform *transforms, int transforms_n ) {
	ctx->transforms = transforms;
	ctx->transforms_n = transforms_n;
}

void grn_ctx_set_transforms_v( struct grn_ctx *ctx, struct vector *transforms ) {
	ctx->transforms = ( struct grn_transform * ) vector_export( transforms, &ctx->transforms_n );
}

int bencode_error_to_anb( int bencode_error ) {
	if ( bencode_error == BEN_OK ) return GRN_OK;
	if ( bencode_error == BEN_NO_MEMORY ) return GRN_ERR_OOM;
	return GRN_ERR_BENCODE_SYNTAX;
}

// END custom data type operations

// BEGIN preset and semi-presets

// some constants first
const char OPS_PASSPHRASE_PREFIX[] = "https://opsfet.ch/";
const char OPS_PASSPHRASE_SUFFIX[] = "/announce";
const int OPS_PASSPHRASE_LENGTH = 32;
const int OPS_URL_LENGTH = 60;

char *announce_str_key[] = {
	"announce",
	NULL,
};
char *announce_list_key[] = {
	"announce-list",
	"",
	NULL,
};
char *utorrent_trackers_key[] = {
	"",
	"trackers",
	"",
	NULL,
};
char *qbittorrent_fastresume_trackers_key[] = {
	"trackers",
	"",
	"",
	NULL,
};

enum {
	GRN_PASSPHRASE_NOT = 0,
	GRN_PASSPHRASE_EXACT,
	GRN_PASSPHRASE_STARTS,
};

int is_string_passphrase( const char *str ) {
	return strspn( str, "0123456789abcdef" ) >= OPS_PASSPHRASE_LENGTH ?
	       strlen( str ) == OPS_PASSPHRASE_LENGTH ? GRN_PASSPHRASE_EXACT : GRN_PASSPHRASE_STARTS
	       : GRN_PASSPHRASE_NOT;
}

// Not thread safe, but neither am I!
/**
* @brief Converts either an OPS announce URL or an OPS passphrase into a URL, or errors out
*
* @param user_announce The input from the user
* @param our_announce A string of size OPS_URL_LENGTH plus null byte. Will be written to.
* @return whether we were able to normalize or not. If false, bad user input.
*/
bool normalize_announce_url( char *user_announce, char *our_announce ) {

	if ( user_announce == NULL ) {
		return false;
	}

	if ( is_string_passphrase( user_announce ) == GRN_PASSPHRASE_EXACT ) {
		strcpy( our_announce, OPS_PASSPHRASE_PREFIX );
		strcat( our_announce, user_announce );
		strcat( our_announce, OPS_PASSPHRASE_SUFFIX );
		return true;
	}

	if (
	    // https://opsfet.ch/
	    strncmp( user_announce, OPS_PASSPHRASE_PREFIX, strlen( OPS_PASSPHRASE_PREFIX ) ) == 0 &&
	    // passphrase
	    is_string_passphrase( user_announce + strlen( OPS_PASSPHRASE_PREFIX ) ) == GRN_PASSPHRASE_STARTS &&
	    // /announce
	    strcmp( user_announce + strlen( OPS_PASSPHRASE_PREFIX ) + OPS_PASSPHRASE_LENGTH, OPS_PASSPHRASE_SUFFIX ) == 0
	) {
		strcpy( our_announce, user_announce );
		return true;
	}

	return false;
}

// generic orpheus transform, i.e, user did not provide a passphrase
void grn_cat_transforms_orpheus( struct vector *vec, char *user_announce, int *out_err ) {
	*out_err = GRN_OK;

	char *normalized_url = malloc( OPS_URL_LENGTH );
	ERR( normalized_url == NULL, GRN_ERR_OOM );

	if ( !normalize_announce_url( user_announce, normalized_url ) ) {
		free( normalized_url );
		*out_err = GRN_ERR_ORPHEUS_ANNOUNCE_SYNTAX;
		return;
	}
	GRN_LOG_DEBUG( "Normalized announce URL: %s", normalized_url );

	// there's no fucking way this should be dynamically allocated, but it is.
	struct grn_transform key_subst, list_subst, ut_subst, qb_subst;

	key_subst = grn_mktransform_substitute_regex( "^https?:\\/\\/(mars\\.)?(apollo|xanax)\\.rip(:2095)?\\/[a-f0-9]{32}\\/announce\\/?$", normalized_url, out_err );
	ERR_FW();
	key_subst.dynamalloc = GRN_DYNAMIC_TRANSFORM_FIRST | GRN_DYNAMIC_TRANSFORM_SECOND;
	list_subst = key_subst;
	list_subst.dynamalloc = 0;
	ut_subst = key_subst;
	ut_subst.dynamalloc = 0;
	qb_subst = key_subst;
	qb_subst.dynamalloc = 0;

	key_subst.key = announce_str_key;
	list_subst.key = announce_list_key;
	ut_subst.key = utorrent_trackers_key;
	qb_subst.key = qbittorrent_fastresume_trackers_key;

	vector_push( vec, &key_subst, out_err );
	ERR_FW();
	vector_push( vec, &list_subst, out_err );
	ERR_FW();
	vector_push( vec, &ut_subst, out_err );
	ERR_FW();
	vector_push( vec, &qb_subst, out_err );
	ERR_FW();
}

struct grn_transform grn_mktransform_set_string( char *key, char *val ) {
	return ( struct grn_transform ) {
		.operation = GRN_TRANSFORM_SET_STRING,
		.payload = {
			.set_string = {
				.key = key,
				.val = val,
			},
		},
		.dynamalloc = 0,
	};
}

struct grn_transform grn_mktransform_delete( char *key ) {
	return ( struct grn_transform ) {
		.operation = GRN_TRANSFORM_DELETE,
		.payload = {
			.set_string = {
				.key = key,
			},
		},
		.dynamalloc = 0,
	};
}

struct grn_transform grn_mktransform_substitute( char *find, char *replace ) {
	return ( struct grn_transform ) {
		.operation = GRN_TRANSFORM_SUBSTITUTE,
		.payload = {
			.substitute = {
				.find = find,
				.replace = replace,
			},
		},
		.dynamalloc = 0,
	};
}

struct grn_transform grn_mktransform_substitute_regex( char *find_regstr, char *replace, int *out_err ) {
	*out_err = GRN_OK;

	struct grn_transform to_return = {
		.operation = GRN_TRANSFORM_SUBSTITUTE_REGEX,
		.payload = {
			.substitute_regex = {
				.replace = replace,
			},
		},
		.dynamalloc = GRN_DYNAMIC_TRANSFORM_FIRST,
	};

	int regcomp_res = regcomp( &to_return.payload.substitute_regex.find, find_regstr, REG_EXTENDED );
	if ( regcomp_res == REG_ESPACE ) {
		*out_err = GRN_ERR_OOM;
		return to_return;
	}
	if ( regcomp_res ) {
		*out_err = GRN_ERR_REGEX_SYNTAX;
		return to_return;
	}

	return to_return;
}

// this feels like such overkill for such a simple struct, but oh well
void grn_free_transform( struct grn_transform *transform ) {
	const int bits = transform->dynamalloc;
	if ( bits & GRN_DYNAMIC_TRANSFORM_KEY_ELEMENTS ) {
		for ( int i = 0; transform->key[i] != NULL; i++ ) {
			free( transform->key[i] );
		}
	}
	if ( bits & GRN_DYNAMIC_TRANSFORM_KEY ) {
		free( transform->key );
	}
	if ( bits & GRN_DYNAMIC_TRANSFORM_FIRST ) {
		if ( transform->operation == GRN_TRANSFORM_SUBSTITUTE_REGEX ) {
			regfree( &transform->payload.substitute_regex.find );
		} else {
			free( transform->payload.delete_.key );
		}
	}
	if ( bits & GRN_DYNAMIC_TRANSFORM_SECOND ) {
		if ( transform->operation == GRN_TRANSFORM_SUBSTITUTE_REGEX ) {
			free( transform->payload.substitute_regex.replace );
		} else {
			free( transform->payload.substitute.replace );
		}
	}
	if ( bits & GRN_DYNAMIC_TRANSFORM_SELF ) {
		free( transform );
	}
}

void grn_free_transforms_v( struct vector *vec ) {
	assert( vec != NULL );
	int transforms_n;
	struct grn_transform *transforms = vector_export( vec, &transforms_n );
	for ( int i = 0; i < transforms_n; i++ ) {
		grn_free_transform( &transforms[i] );
	}
	free( transforms );
}

// END preset and semi-presets

/**
* @brief replaces in a string based on the index of the start and end of the needle
*
* @param haystack the string to replace inside of
* @param replace the string to replace with
* @param start_i The zero-indexed start of the found needle
* @param end_i The zero-indexed end of the needl in haystack
* @return the dynamically-allocated replaced haystack with replacements
*/
char *strsubst_by_indices( const char *haystack, const char *replace, int start_i, int find_n, int *out_err ) {
	*out_err = GRN_OK;

	assert( start_i >= 0 );
	assert( find_n >= 0 );
	const int haystack_n = strlen( haystack ),
	          replace_n = strlen( replace );

	char *to_return = malloc( haystack_n + replace_n + 1 );
	ERR_NULL( to_return == NULL, GRN_ERR_OOM );
	to_return[0] = '\0';
	strncat( to_return, haystack, start_i );

	// copy substitution
	// intentionally ignore null byte on replace
	strcat( to_return, replace );

	// copy suffix
	strcat( to_return, haystack + start_i + find_n );

	return to_return;
}

// returns dynamically allocated
char *strsubst( const char *haystack, const char *find, const char *replace, int *out_err ) {
	*out_err = GRN_OK;
	char *to_return;

	char *needle_start = strstr( haystack, find );
	// not contained
	if ( needle_start == NULL ) {
		to_return = malloc( strlen( haystack ) + 1 );
		ERR_NULL( to_return == NULL, GRN_ERR_OOM );
		strcpy( to_return, haystack );
		RETURN_OK( to_return );
	}
	to_return = strsubst_by_indices( haystack, replace, needle_start - haystack, strlen( find ), out_err );
	ERR_FW_NULL();
	return to_return;
}

char *regsubst( const char *haystack, regex_t *find, const char *replace, int *out_err ) {
	*out_err = GRN_OK;
	regmatch_t match[1];
	char *to_return;

	int regexec_res = regexec( find, haystack, 1, match, 0 );
	// supposedly it can only fail in case of no match -- not OOM
	if ( regexec_res ) {
		to_return = malloc( strlen( haystack ) + 1 );
		ERR_NULL( to_return == NULL, GRN_ERR_OOM );
		strcpy( to_return, haystack );
		return to_return;
	}
	to_return = strsubst_by_indices( haystack, replace, match->rm_so, match->rm_eo - match->rm_so, out_err );
	ERR_FW_NULL();
	return to_return;
}

/**
 * @param haystack the string that should end with needle
 * @param needle the string haystack should end with
 * @return true if haystack ends with needle
 */
bool str_ends_with( const char *haystack, const char *needle ) {
	int haystack_n = strlen( haystack );
	int needle_n = strlen( needle );
	const char *haystack_suffix = haystack + haystack_n - needle_n;
	return strcmp( haystack_suffix, needle ) == 0;
}


// wrappers for ben functions that use our error idioms
// putting `grn` at the end rather than the beginning denotes that it is not public, but still grn-specific

void ben_free_grn( struct bencode *ben ) {
	if ( ben != NULL ) {
		ben_free( ben );
	}
}

struct bencode *ben_decode_grn( const void *buffer, size_t buffer_n, int *out_err ) {
	*out_err = GRN_OK;

	int bencode_error;
	size_t off = 0;
	struct bencode *to_return = ben_decode2( buffer, buffer_n, &off, &bencode_error );
	if ( bencode_error ) {
		// TODO: rename anb
		*out_err = bencode_error_to_anb( bencode_error );
		ben_free_grn( to_return );
		return NULL;
	}

	return to_return;
}

void *ben_encode_grn( struct bencode *ben, size_t *buffer_n, int *out_err ) {
	void *to_return = ben_encode( buffer_n, ben );
	ERR_NULL( to_return == NULL, GRN_ERR_OOM );
	return to_return;
}

/**
 * Replace a bencode string with a different one, in-place
 * The previous string will be freed.
 * @param ben the bencode string object to mutate
 * @param replace_with the string to use as the new one. *must* be dynamically allocated.
 */
void ben_str_swap( struct bencode *ben, char *replace_with ) {
	assert( ben->type == BENCODE_STR );
	struct bencode_str *benstr = ( struct bencode_str * ) ben;
	free( benstr->s );
	benstr->s = replace_with;
	benstr->len = strlen( replace_with );
}

void mutate_string_subst( struct bencode *ben, struct grn_op_substitute payload, int *out_err ) {
	*out_err = GRN_OK;
	if ( ben->type != BENCODE_STR ) {
		return;
	}
	GRN_LOG_DEBUG( "Substituting %s for %s", payload.find, payload.replace );

	char *substituted = strsubst( ben_str_val( ben ), payload.find, payload.replace, out_err );
	ERR_FW();
	ben_str_swap( ben, substituted );
}

void mutate_string_subst_regex( struct bencode *ben, struct grn_op_substitute_regex payload, int *out_err ) {
	*out_err = GRN_OK;
	if ( ben->type != BENCODE_STR ) {
		return;
	}

	char *substituted = regsubst( ben_str_val( ben ), &payload.find, payload.replace, out_err );
	ERR_FW();
	ben_str_swap( ben, substituted );
}

void cat_descendants( struct vector *vec, struct bencode *ben, int *out_err ) {
	*out_err = GRN_OK;

	struct bencode *key, *val;
	size_t pos;
	switch ( ben->type ) {
		case BENCODE_DICT:
			;
			ben_dict_for_each( key, val, pos, ben ) {
				vector_push( vec, &val, out_err );
				ERR_FW();
			}
			break;
		case BENCODE_LIST:
			;
			ben_list_for_each( val, pos, ben ) {
				vector_push( vec, &val, out_err );
				ERR_FW();
			}
			break;
		default:
			;
			break;
	}
}

// transforms a buffer based on a single transform and does not filter
void transform_buffer_single( struct bencode *ben, struct grn_transform transform, int *out_err ) {
	*out_err = GRN_OK;

	GRN_LOG_DEBUG( "Executing transform, %d", transform.operation );
	switch ( transform.operation ) {
		case GRN_TRANSFORM_DELETE:
			;
			// it returns a "standalone" pointer to the value, that must be freed. It modifies
			// the main_dict in place
			if ( ben->type != BENCODE_DICT ) {
				break;
			}
			struct bencode *popped_val = ben_dict_pop_by_str( ben, transform.payload.delete_.key );
			if ( popped_val != NULL ) {
				ben_free( popped_val );
			}
			break;
		case GRN_TRANSFORM_SET_STRING:
			;
			if ( ben->type != BENCODE_DICT ) {
				break;
			}
			struct grn_op_set_string setstr_payload = transform.payload.set_string;
			ERR( ben_dict_set_str_by_str( ben, setstr_payload.key, setstr_payload.val ), GRN_ERR_OOM );
			break;
		case GRN_TRANSFORM_SUBSTITUTE:
			;
			mutate_string_subst( ben, transform.payload.substitute, out_err );
			ERR_FW();
			break;
		case GRN_TRANSFORM_SUBSTITUTE_REGEX:
			;
			mutate_string_subst_regex( ben, transform.payload.substitute_regex, out_err );
			ERR_FW();
			break;
		default:
			;
			assert( false );
			break;
	}
}

void transform_buffer( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;

	assert( ctx->state == GRN_CTX_TRANSFORM );

	// TODO: this
	struct vector *f_to_traverse = vector_alloc( sizeof( struct bencode * ), out_err );
	ERR_FW_CLEANUP();
	struct vector *f_traversing = vector_alloc( sizeof( struct bencode * ), out_err );
	ERR_FW_CLEANUP();
	struct vector *f_out;

	struct bencode *main_dict = ben_decode_grn( ctx->buffer, ctx->buffer_n, out_err );
	ERR_FW_CLEANUP();

	for ( int i = 0; i < ctx->transforms_n; i++ ) {
		struct grn_transform transform = ctx->transforms[i];
		assert( transform.key != NULL );

		// first, filter down by the keys in the transform
		vector_clear( f_to_traverse );
		vector_clear( f_traversing );
		vector_push( f_to_traverse, &main_dict, out_err );
		ERR_FW_CLEANUP();

		char *filter_key;
		int k = 0;
		while ( ( filter_key = transform.key[k++] ) != NULL ) {
			GRN_LOG_DEBUG( "Filtering by key: '%s' ", filter_key );
			// essentially, we want to make f_to_traverse empty and start traversing the former to_traverse
			struct vector *f_tmp = f_traversing;
			f_traversing = f_to_traverse;
			f_to_traverse = f_tmp;
			vector_clear( f_to_traverse );

			while ( vector_length( f_traversing ) > 0 ) {
				struct bencode *traversing = * ( struct bencode ** ) vector_pop( f_traversing );

				// wildcard
				if ( filter_key[0] == '\0' ) {
					GRN_LOG_DEBUG( "Performing wildcard filter%s", "" );
					cat_descendants( f_to_traverse, traversing, out_err );
					ERR_FW_CLEANUP();
				} else {
					GRN_LOG_DEBUG( "Non-wildcard filter%s", "" );
					if ( traversing->type != BENCODE_DICT ) {
						GRN_LOG_DEBUG( "Skipping because not a dictionary%s", "" );
						continue;
					}
					struct bencode *maybe_val = ben_dict_get_by_str( traversing, filter_key );
					if ( maybe_val != NULL ) {
						vector_push( f_to_traverse, &maybe_val, out_err );
						ERR_FW_CLEANUP();
					}
				}
			}
		}
		f_out = f_to_traverse;

		while ( vector_length( f_out ) > 0 ) {
			struct bencode *filtered = * ( struct bencode ** ) vector_pop( f_out );
			transform_buffer_single( filtered, transform, out_err );
			if ( *out_err ) {
				goto cleanup;
			}
		}
	}

	free( ctx->buffer );
	ctx->buffer = ben_encode_grn( main_dict, &ctx->buffer_n, out_err );
	ERR_FW_CLEANUP();
	GRN_LOG_DEBUG( "Newly encoded file size: %d", ( int )ctx->buffer_n );
	goto cleanup;
cleanup:
	if ( main_dict != NULL ) {
		ben_free( main_dict );
	}
	vector_free( f_traversing );
	vector_free( f_to_traverse );
	return;
}

/**
 * Truncates the file and opens in writing mode.
 */
void freopen_ctx( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;
	// it will get fclosed by the caller with grn_ctx_free
	FILE *reopened_fh = freopen( NULL, "w", ctx->fh );
	ERR( reopened_fh == NULL, GRN_ERR_FS_OPEN );
	ctx->fh = reopened_fh;
}

// cleanup after a potentially failed single file then proceed to the next file
void next_file_ctx( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;

	GRN_LOG_DEBUG( "Next: file %d to %d", ctx->files_c, ctx->files_c + 1 );
	ctx->files_c++;
	ctx->file_error = GRN_OK;

	grn_free( ctx->buffer );
	ctx->buffer = NULL;
	// close the previously processing file
	if ( ctx->fh != NULL ) {
		// TODO: have a separate GRN_CTX_CLOSE state
		if ( fclose( ctx->fh ) ) {
			*out_err = GRN_ERR_FS_CLOSE;
		}
		ctx->fh = NULL;
	}

	// are we done?
	if ( ctx->files_c >= ctx->files_n ) {
		ctx->state = GRN_CTX_DONE;
		return;
	}

	// prepare the next file for reading
	ERR( ( ctx->fh = fopen( ctx->files[ctx->files_c], "r" ) ) == NULL, GRN_ERR_FS_OPEN );
	ctx->state = GRN_CTX_READ;
}

// BEGIN mainish functions

bool grn_one_step( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;

#define GRN_STEP_ERR() do { \
	if ( *out_err ) { \
		if ( grn_err_is_single_file ( *out_err ) ) { \
			ctx->file_error = *out_err; \
			ctx->errs_n++; \
			ctx->state = GRN_CTX_NEXT; \
			*out_err = GRN_OK; \
		} \
		return false; \
	} \
} while (0)

	GRN_LOG_DEBUG( "Stepping -- current state: %d", ctx->state );
	switch ( ctx->state ) {
		case GRN_CTX_DONE:
			;
			break;
		case GRN_CTX_REOPEN:
			;
			freopen_ctx( ctx, out_err );
			GRN_STEP_ERR();
			ctx->state = GRN_CTX_WRITE;
			break;
		case GRN_CTX_READ:
			;
			// means we should continue reading the current file
			// fread_ctx will only "throw" an error if it's not an FS problem (which indicates file specific problem).
			// TODO: consider and maybe actually do what is described just above
			fread_ctx( ctx, out_err );
			GRN_STEP_ERR();
			ctx->state = GRN_CTX_TRANSFORM;
			// TODO: once we add non-blocking, call grn_one_step again here
			break;
		case GRN_CTX_WRITE:
			;
			fwrite_ctx( ctx, out_err );
			GRN_STEP_ERR();
			ctx->state = GRN_CTX_NEXT;
			break;
		case GRN_CTX_TRANSFORM:
			;
			transform_buffer( ctx, out_err );
			GRN_STEP_ERR();
			ctx->state = GRN_CTX_REOPEN;
			// TODO: run grn_one_step again
			break;
		case GRN_CTX_NEXT:
			;
			// unlike some of the other functions, next_file_ctx internally updates ctx->state
			next_file_ctx( ctx, out_err );
			GRN_STEP_ERR();
			break;
		default:
			;
			assert( false );
			break;
	}

	return ctx->state == GRN_CTX_DONE;
}

bool grn_one_file( struct grn_ctx *ctx, int *out_err ) {
	// essentially: Make sure we're starting right after a file, then run until we are about to start the next file
	*out_err = GRN_OK;

	if ( ctx->state == GRN_CTX_DONE ) {
		return true;
	}
	assert( ctx->state == GRN_CTX_NEXT );
	// it is set to -1 on allocation
	do {
		grn_one_step( ctx, out_err );
		if ( *out_err ) {
			return false;
		}
	} while ( ctx->state != GRN_CTX_NEXT && ctx->state != GRN_CTX_DONE );
	return ctx->state == GRN_CTX_DONE;
}

void grn_one_ctx( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;

	while ( ctx->state != GRN_CTX_DONE ) {
		grn_one_step( ctx, out_err );
		if ( out_err ) {
			return;
		}
	}
}

// END mainish functions


// global because nftw doesn't support a custom callback argument
struct vector *cat_vec;
const char *cat_ext;

// used as nftw callback below
int cat_nftw_cb( const char *path, const struct stat *st, int file_type, struct FTW *ftw_info ) {
	int in_err;

	// ignore non-files and files without the correct extension
	if (
	    file_type != FTW_F ||
	    !str_ends_with( path, cat_ext )
	) {
		return 0;
	}

	// the path might not be dynamic (it might actually change between callback runs, if nftw uses readdir internally?)
	char *path_cp = malloc( strlen( path ) + 1 );
	if ( path_cp == NULL ) {
		return GRN_ERR_OOM;
	}
	strcpy( path_cp, path );
	vector_push( cat_vec, &path_cp, &in_err );
	return in_err;
}

void grn_cat_torrent_files( struct vector *vec, const char *path, const char *extension, int *out_err ) {
	*out_err = GRN_OK;

	cat_vec = vec;
	cat_ext = extension != NULL ? extension : ".torrent";

	int nftw_err = nftw( path, cat_nftw_cb, 16, 0 );
	if ( nftw_err ) {
		// if nftw returns -1, it means there was some internal problem. Any other error means that our cb failed.
		*out_err = nftw_err == -1 ? GRN_ERR_FS_NFTW : nftw_err;
		return;
	}
}

// helper function for use in grn_cat_client
void cat_client_single_path( struct vector *vec, const char *home, const char *sub, const char *extension, int *out_err ) {
	*out_err = GRN_OK;
	assert(vec != NULL);
	assert(home != NULL);
	assert(sub != NULL);

	char *full_path = malloc( strlen( home ) + strlen( sub ) + 1 );
	ERR( full_path == NULL, GRN_ERR_OOM );
	strcpy( full_path, home );
	strcat( full_path, sub );

	if (access( full_path, R_OK | X_OK ) ) {
		*out_err = GRN_ERR_READ_CLIENT_PATH;
		goto cleanup;
	}
	grn_cat_torrent_files( vec, full_path, extension, out_err );
	ERR_FW_CLEANUP();
	goto cleanup;
cleanup:
	grn_free(full_path);
}

/**
 * Here's how different torrent clients handle things:
 *   - Transmission: Uses the on-disk torrent for everything, fuckin' noice m88! The resume file does not store the tracker.
 *   - Deluge: Has a global fastresume file at ~/.config/deluge/torrents.fastresume in bencode format. The keys are info hashes, but, i fuck you not, the values are strings of bencode that must be parsed! In each of those sub-dictionaries, the "trackers" array contains the ~~good~~ bad shit.
 *   - qBittorrent: Has separate fastresume files in the same folder as the main torrent. The "trackers" key must be modified.
 *   - uTorrent is also bencode. Each key in the root dict is the name of a .torrent file. Inside is a "trackers" list.
 */
void grn_cat_client( struct vector *vec, int client, int *out_err ) {
	*out_err = GRN_OK;

	// TODO: does this work in Windows? Should we get appdata instead?
	char *home_path = getenv( "HOME" );
	ERR( home_path == NULL, GRN_ERR_NO_CLIENT_PATH );

	switch ( client ) {
		case GRN_CLIENT_QBITTORRENT:
			;
#if defined __unix__
			cat_client_single_path(vec, home_path, "/.local/share/data/qBittorrent/BT_backup", ".torrent", out_err);
			ERR_FW();
			cat_client_single_path(vec, home_path, "/.local/share/data/qBittorrent/BT_backup", ".fastresume", out_err);
			ERR_FW();
#elif defined __APPLE__
			cat_client_single_path(vec, home_path, "/Library/Application Suppport/qBittorrent/BT_backup", ".torrent", out_err);
			ERR_FW();
			cat_client_single_path(vec, home_path, "/Library/Application Suppport/qBittorrent/BT_backup", ".fastresume", out_err);
			ERR_FW();
#elif defined _WIN32
			cat_client_single_path(vec, home_path, "/AppData/Local/qBittorrent/BT_backup", ".torrent", out_err);
			ERR_FW();
			cat_client_single_path(vec, home_path, "/AppData/Local/qBittorrent/BT_backup", ".fastresume", out_err);
			ERR_FW();
#endif
			break;
		case GRN_CLIENT_TRANSMISSION:
			;
#if defined __unix__
			cat_client_single_path(vec, home_path, "/.config/transmission/torrents", ".torrent", out_err);
			ERR_FW();
#elif defined __APPLE__
			cat_client_single_path(vec, home_path, "/Library/Application Support/Transmission/torrents", ".torrent", out_err);
			ERR_FW();
#elif defined _WIN32
			cat_client_single_path(vec, home_path, "/AppData/Local/transmission/torrents", ".torrent", out_err);
			ERR_FW();
#endif
			break;
		case GRN_CLIENT_TRANSMISSION_DAEMON:
			;
#if defined __unix__
			cat_client_single_path(vec, home_path, "/.config/transmission-daemon/torrents", ".torrent", out_err);
			ERR_FW();
#elif defined __APPLE__
			cat_client_single_path(vec, home_path, "/Library/Application Support/Transmission/torrents", ".torrent", out_err);
			ERR_FW();
			// TODO: check what the status is of transmission daemon on mac. Does it exist at all?
#elif defined _WIN32
			cat_client_single_path(vec, home_path, "/AppData/Local/transmission-daemon/torrents", ".torrent", out_err);
			ERR_FW();
#endif
			break;
#if defined _WIN32
		case GRN_CLIENT_UTORRENT:
			;
			cat_client_single_path(vec, home_path, "/AppData/Roaming/uTorrent/", ".torrent", out_err);
			ERR_FW();
			cat_client_single_path(vec, home_path, "/AppData/Roaming/uTorrent/resume.dat", ".dat", out_err);
			ERR_FW();
			break;
#endif
		default:
			assert( false );
			return;
			break;
	}
}

// BEGIN get info

bool grn_ctx_get_is_done( struct grn_ctx *ctx ) {
	return ctx->state == GRN_CTX_DONE;
}

char *grn_ctx_get_c_path( struct grn_ctx *ctx ) {
	assert( ctx->files_c >= 0 );
	assert( ctx->files_c < ctx->files_n );
	return ctx->files[ctx->files_c];
}

char *grn_ctx_get_next_path( struct grn_ctx *ctx ) {
	if ( ctx->files_c + 1 < ctx->files_n ) {
		return ctx->files[ctx->files_c + 1];
	} else {
		return NULL;
	}
}

int grn_ctx_get_c_error( struct grn_ctx *ctx ) {
	assert( ctx->files_c >= 0 );
	return ctx->file_error;
}

int grn_ctx_get_files_c( struct grn_ctx *ctx ) {
	return ctx->files_c + 1;
}

int grn_ctx_get_files_n( struct grn_ctx *ctx ) {
	return ctx->files_n;
}

int grn_ctx_get_errs_n( struct grn_ctx *ctx ) {
	return ctx->errs_n;
}

// END get info



