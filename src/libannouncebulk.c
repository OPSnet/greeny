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
#include "err.h"
// are you supposed to do this?

// BEGIN macros


// END macros

// BEGIN context filesystem

void fread_ctx( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;
	assert( ctx->state == GRN_CTX_READ );

	// we can't just do fread(buffer, 1, some_massive_num, fh) because we can't be sure whether
	// the whole file was read or not.
	ERR( fseek( ctx->fh, 0, SEEK_END ), GRN_ERR_FS );
	ctx->buffer_n = ftell( ctx->fh );
	ERR( fseek( ctx->fh, 0, SEEK_SET ), GRN_ERR_FS );

	ctx->buffer = malloc( ctx->buffer_n );
	ERR( ctx->buffer == NULL, GRN_ERR_OOM );
	if ( fread( ctx->buffer, ctx->buffer_n, 1, ctx->fh ) != 1 ) {
		free( ctx->buffer );
		ERR( GRN_ERR_FS );
	}
}

void fwrite_ctx( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;
	assert( ctx->state == GRN_CTX_WRITE );

	ERR( fwrite( ctx->buffer, ctx->buffer_n, 1, ctx->fh ) != 1, GRN_ERR_FS );
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
	free( ctx->files );
	if ( ctx->buffer != NULL ) {
		free( ctx->buffer );
	}
	if ( ctx->fh != NULL ) {
		ERR( fclose( ctx->fh ), GRN_ERR_FS );
	}
	RETURN_OK();
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

void grn_ctx_set_transforms_v( struct grn_ctx *ctx, struct vector *transforms, int *out_err ) {
	*out_err = GRN_OK;

	// THE TRANSFORMS ARE NOT IN CONTIGUOUs MEMORY ONLY THE POINTERS AHHHHH// THE TRANSFORMS ARE NOT IN CONTIGUOUs MEMORY ONLY THE POINTERS AHHHHH// THE TRANSFORMS ARE NOT IN CONTIGUOUs MEMORY ONLY THE POINTERS AHHHHH
	// ^^^ i'm not sure what key i hit to duplicate it but i think i will let it stay
	struct grn_transform **ptrs = ( struct grn_transform ** ) vector_export( transforms, &ctx->transforms_n );
	ctx->transforms = grn_flatten_ptrs( ( void ** )ptrs, ctx->transforms_n, sizeof( struct grn_transform ), out_err );
	ERR_FW();
}

char *grn_err_to_string( int err ) {
	static char *err_strings[] = {
		"Successful.",
		"Out of memory.",
		"Filesystem error.",
		"Wrong bencode type (eg, int where string expected).",
		"Wrong transform operation (unknown).",
		"Context was in an unexpected state.",
		"An unknown bittorrent client was specified.",
		"Bencode syntax error.",
		"Could not determine the path for the given bittorrent client.",
		"Could not access the path for the given bittorrent client.",
		"Invalid regular expression.",
		"Orpheus passphrase/announce URL was invalid.",
	};
	return err_strings[err];
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
const int OPS_URL_LENGTH = 59;

char *announce_str_key[] = {
	"announce",
	NULL,
};
char *announce_list_key[] = {
	"announce-list",
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
* @param our_announce A string of size OPS_URL_LENGTH plus null byte
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

	ERR( !normalize_announce_url( user_announce, normalized_url ), GRN_ERR_ORPHEUS_ANNOUNCE_SYNTAX );
	GRN_LOG_DEBUG( "Normalized announce URL: %s", normalized_url );

	// there's no fucking way this should be dynamically allocated, but it is.
	struct grn_transform *key_subst = malloc( sizeof( struct grn_transform ) );
	ERR( key_subst == NULL, GRN_ERR_OOM );
	struct grn_transform *list_subst = malloc( sizeof( struct grn_transform ) );
	ERR( list_subst == NULL, GRN_ERR_OOM );

	*key_subst = grn_mktransform_substitute_regex( "^https?:\\/\\/(mars\\.)?(apollo|xanax)\\.rip(:2095)?\\/[a-f0-9]{32}\\/announce\\/?$", normalized_url, out_err );
	ERR_FW();
	key_subst->dynamalloc = GRN_DYNAMIC_TRANSFORM_SELF | GRN_DYNAMIC_TRANSFORM_FIRST | GRN_DYNAMIC_TRANSFORM_SECOND;
	*list_subst = *key_subst;
	list_subst->dynamalloc = GRN_DYNAMIC_TRANSFORM_SELF | GRN_DYNAMIC_TRANSFORM_SECOND;

	key_subst->key = announce_str_key;
	list_subst->key = announce_list_key;

	vector_push( vec, key_subst, out_err );
	ERR_FW();
	vector_push( vec, list_subst, out_err );
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
	for ( int i = 0; i < vec->used_n; i++ ) {
		grn_free_transform( vec->buffer[i] );
	}
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

void ben_str_swap( struct bencode *ben, char *replace_with ) {
	struct bencode_str *benstr = ( struct bencode_str * ) ben;
	free( benstr->s );
	benstr->s = replace_with;
	benstr->len = strlen( replace_with );
}

// these two are mutators
void mutate_string_subst( struct bencode *ben, void *state, int *out_err ) {
	*out_err = GRN_OK;
	assert( ben->type == BENCODE_STR );

	struct grn_op_substitute *payload = ( struct grn_op_substitute * ) state;
	char *substituted = strsubst( ben_str_val( ben ), payload->find, payload->replace, out_err );
	ERR_FW();
	ben_str_swap( ben, substituted );
}

void mutate_string_subst_regex( struct bencode *ben, void *state, int *out_err ) {
	*out_err = GRN_OK;
	assert( ben->type == BENCODE_STR );

	struct grn_op_substitute_regex *payload = ( struct grn_op_substitute_regex * ) state;
	char *substituted = regsubst( ben_str_val( ben ), &payload->find, payload->replace, out_err );
	ERR_FW();
	ben_str_swap( ben, substituted );
}

/**
* @brief Transforms a bencode by running a custom callback for each string, recursively
*
* @param ben The bencode to modify
* @param mutator The custom callback
* @param mutate_me A bencode string
* @param state custom param
*/
void ben_forall_strings( struct bencode *ben, void ( *mutator )( struct bencode *mutate_me, void *state, int *out_err ), void *state, int *out_err ) {
	*out_err = GRN_OK;

	if ( ben->type == BENCODE_STR ) {
		mutator( ben, state, out_err );
		ERR_FW();
	} else if ( ben->type == BENCODE_LIST ) {
		int list_n = ben_list_len( ben );
		for ( int i = 0; i < list_n; i++ ) {
			struct bencode *list_cur = ben_list_get( ben, i );
			ben_forall_strings( list_cur, mutator, state, out_err );
			ERR_FW();
		}
	}
}

void transform_buffer( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;

	assert( ctx->state == GRN_CTX_TRANSFORM );

	int bencode_error;
	size_t off = 0;
	struct bencode *main_dict = ben_decode2( ctx->buffer, ctx->buffer_n, &off, &bencode_error );
	ERR( bencode_error, bencode_error_to_anb( bencode_error ) );

	for ( int i = 0; i < ctx->transforms_n; i++ ) {
		struct grn_transform transform = ctx->transforms[i];

		// first, filter down by the keys in the transform
		struct bencode *filtered = main_dict;
		assert( transform.key != NULL );
		char *filter_key;
		int k = 0;
		while ( ( filter_key = transform.key[k++] ) != NULL ) {
			if ( filtered == NULL || filtered->type != BENCODE_DICT ) {
				break;
			}
			filtered = ben_dict_get_by_str( filtered, filter_key );
		}
		if ( filtered == NULL ) {
			continue;
		}

		switch ( transform.operation ) {
			case GRN_TRANSFORM_DELETE:
				;
				// it returns a "standalone" pointer to the value, that must be freed. It modifies
				// the main_dict in place
				if ( filtered->type != BENCODE_DICT ) {
					break;
				}
				struct bencode *popped_val = ben_dict_pop_by_str( filtered, transform.payload.delete_.key );
				if ( popped_val != NULL ) {
					ben_free( popped_val );
				}
				break;
			case GRN_TRANSFORM_SET_STRING:
				;
				if ( filtered->type != BENCODE_DICT ) {
					break;
				}
				struct grn_op_set_string setstr_payload = transform.payload.set_string;
				ERR( ben_dict_set_str_by_str( filtered, setstr_payload.key, setstr_payload.val ), GRN_ERR_OOM );
				break;
			case GRN_TRANSFORM_SUBSTITUTE:
				;
				ben_forall_strings( filtered, &mutate_string_subst, ( void * ) &transform.payload, out_err );
				ERR_FW();
				break;
			case GRN_TRANSFORM_SUBSTITUTE_REGEX:
				;
				ben_forall_strings( filtered, &mutate_string_subst_regex, ( void * ) &transform.payload, out_err );
				ERR_FW();
				break;
			default:
				;
				ERR( GRN_ERR_WRONG_TRANSFORM_OPERATION );
				break;
		}
	}

	free( ctx->buffer );
	ctx->buffer = ben_encode( &ctx->buffer_n, main_dict );
	ERR( ctx->buffer == NULL, GRN_ERR_OOM );
	goto cleanup;
cleanup:
	ben_free( main_dict );
	return;
}

/**
 * Truncates the file and opens in writing mode.
 */
void freopen_ctx( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;
	// it will get fclosed by the caller with grn_ctx_free
	ctx->fh = freopen( NULL, "w", ctx->fh );
	ERR( ctx->fh == NULL, GRN_ERR_FS );
}

void next_file_ctx( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;

	// close the previously processing file
	if ( ctx->fh ) {
		ERR( fclose( ctx->fh ), GRN_ERR_FS );
	}

	ctx->files_c++;
	// are we done?
	if ( ctx->files_c >= ctx->files_n ) {
		ctx->state = GRN_CTX_DONE;
		return;
	}

	// prepare the next file for reading
	ERR( !( ctx->fh = fopen( ctx->files[ctx->files_c], "r" ) ), GRN_ERR_FS );
	ctx->state = GRN_CTX_READ;
}

// BEGIN mainish functions

bool grn_one_step( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;

	switch ( ctx->state ) {
		case GRN_CTX_DONE:
			;
			return true;
			break;
		case GRN_CTX_REOPEN:
			;
			freopen_ctx( ctx, out_err );
			if ( *out_err ) {
				return false;
			}
			ctx->state = GRN_CTX_WRITE;
			return false;
			break;
		case GRN_CTX_READ:
			;
			// means we should continue reading the current file
			// fread_ctx will only "throw" an error if it's not an FS problem (which indicates file specific problem).
			// TODO: consider and maybe actually do what is described just above
			fread_ctx( ctx, out_err );
			// fuckin macros
			if ( *out_err ) {
				// mostly just to be safe, since we don't reassign later it would probably be ok without this.
				return false;
			}
			ctx->state = GRN_CTX_TRANSFORM;
			return false;
			// TODO: once we add non-blocking, call grn_one_step again here
			break;
		case GRN_CTX_WRITE:
			;
			fwrite_ctx( ctx, out_err );
			ctx->state = GRN_CTX_NEXT;
			return false;
			break;
		case GRN_CTX_TRANSFORM:
			;
			transform_buffer( ctx, out_err );
			ctx->state = GRN_CTX_REOPEN;
			// TODO: run grn_one_step again
			return false;
			break;
		case GRN_CTX_NEXT:
			;
			// unlike some of the other functions, next_file_ctx internally updates ctx->state
			next_file_ctx( ctx, out_err );
			if ( *out_err ) {
				return false;
			}
			return ctx->state == GRN_CTX_DONE;
			break;
		default:
			;
			*out_err = GRN_ERR_WRONG_CTX_STATE;
			return false;
			break;
	}
}

bool grn_one_file( struct grn_ctx *ctx, int *out_err ) {
	*out_err = GRN_OK;

	// it is set to -1 on allocation
	int files_c_old = ctx->files_c == -1 ? 0 : ctx->files_c;
	while ( ctx->files_c <= files_c_old && ctx->state != GRN_CTX_DONE ) {
		grn_one_step( ctx, out_err );
		if ( *out_err ) {
			return false;
		}
	}
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
	vector_push( cat_vec, path_cp, &in_err );
	return in_err;
}

void grn_cat_torrent_files( struct vector *vec, const char *path, const char *extension, int *out_err ) {
	*out_err = GRN_OK;

	cat_vec = vec;
	cat_ext = extension != NULL ? extension : ".torrent";

	int nftw_err = nftw( path, cat_nftw_cb, 16, 0 );
	if ( nftw_err ) {
		// if nftw returns -1, it means there was some internal problem. Any other error means that our cb failed.
		*out_err = nftw_err == -1 ? GRN_ERR_FS : nftw_err;
		return;
	}
}

void grn_cat_client( struct vector *vec, int client, int *out_err ) {
	*out_err = GRN_OK;

	char *state_path = NULL, *home_path, *full_path;
	bool use_home = true;

	// TODO: does this work in Windows? Should we get appdata instead?
	home_path = getenv( "HOME" );
	ERR( home_path == NULL, GRN_ERR_NO_CLIENT_PATH );

	switch ( client ) {
		case GRN_CLIENT_QBITTORRENT:
			;
#if defined __unix__
			state_path = "/.local/share/data/qBittorrent/BT_backup";
#elif defined __APPLE__
			state_path = "/Library/Application Suppport/qBittorrent/BT_backup";
#elif defined _WIN32
			state_path = "/AppData/Local/qBittorrent/BT_backup";
#endif
			break;
		case GRN_CLIENT_DELUGE:
			;
#if defined __unix__ || defined __APPLE__
			state_path = "/.config/deluge/state";
#elif defined _WIN32
			state_path = "";
#endif
			break;
		case GRN_CLIENT_TRANSMISSION:
			;
#if defined __unix__
			state_path = "/.config/transmission/torrents";
#elif defined __APPLE__
			state_path = "/Library/Application Support/Transmission/torrents";
#elif defined _WIN32
			state_path = "/AppData/Local/transmission/torrents";
#endif
			break;
		case GRN_CLIENT_TRANSMISSION_DAEMON:
			;
#if defined __unix__
			state_path = "/.config/transmission-daemon/torrents";
#elif defined __APPLE__
			// TODO: check what the status is of transmission daemon on mac. Does it exist at all?
			state_path = "/Library/Application Support/Transmission/torrents";
#elif defined _WIN32
			state_path = "/AppData/Local/transmission-daemon";
#endif
			break;
		default:
			*out_err = GRN_ERR_WRONG_CLIENT;
			return;
			break;
	}

	// unsupported OS (windows with rtorrent) or something esotoric that does not define our variables
	ERR( state_path == NULL, GRN_ERR_NO_CLIENT_PATH );

	if ( use_home ) {
		full_path = malloc( strlen( home_path ) + strlen( state_path ) + 1 );
		ERR( full_path == NULL, GRN_ERR_OOM );
		strcpy( full_path, home_path );
		strcat( full_path, state_path );
	} else {
		full_path = state_path;
	}

	ERR( access( full_path, R_OK | X_OK ), GRN_ERR_READ_CLIENT_PATH );
	grn_cat_torrent_files( vec, full_path, NULL, out_err );
	// TODO: better errors for catting
	ERR_FW();
}

