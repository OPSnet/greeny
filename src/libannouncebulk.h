#ifndef H_LIBANNOUNCEBULK
#define H_LIBANNOUNCEBULK

#include <stdio.h>
#include <stdbool.h>
#include <regex.h>

#include "vector.h"

// initialize GRN (compiles apollo regex)
void grn_open(int *out_err);
// de-initialize GRN
void grn_close();

int ben_error_to_anb( int bencode_error );

enum {
	GRN_UTRANS_ANNOUNCE_SUBSTITUTE,
	GRN_UTRANS_ANNOUNCE_SUBSTITUTE_REGEX,
};

enum grn_utrans_dynamic {
	GRN_DYNAMIC_TRANSFORM_SELF = 1,
	GRN_DYNAMIC_TRANSFORM_FIND = 2, // first element of payload, whether it's key or find
	GRN_DYNAMIC_TRANSFORM_REPLACE = 4, // second element of payload, whether it's val or replace
};

struct grn_utrans {
	int type;
	int dynamalloc;
};
struct grn_utrans_announce_substitute {
	int type;
	int dynamalloc;
	// putting replace first makes it more consistent to free
	char *replace;
	char *find;
};
struct grn_utrans_announce_substitute_regex {
	int type;
	int dynamalloc;
	char *replace;
	regex_t find;
};

bool grn_mk_orpheus_utrans(const char *user_announce, struct grn_utrans_announce_substitute_regex *utrans);

enum {
	GRN_USRC_RECURSIVE_TORRENT,
	GRN_USRC_TORRENT,
	GRN_USRC_DELUGE,
	GRN_USRC_QBITTORRENT,
	GRN_USRC_TRANSMISSION,
	GRN_USRC_TRANSMISSION_DAEMON,
	GRN_USRC_UTORRENT,
};

struct grn_user_source {
	int type;
	char *path;
};

enum {
	GRN_SRC_TORRENT,
	GRN_SRC_DELUGE_STATE,
	GRN_SRC_QBITTORRENT_FASTRESUME,
	GRN_SRC_UTORRENT_RESUME,
};

struct grn_source {
	int type;
	char *path;
};

void grn_free_srcs( struct grn_source *srcs, int srcs_n );

struct grn_source *grn_usrcs_to_srcs( struct grn_user_source *usrcs, int usrcs_n, int *srcs_n, int *out_err );

void grn_transform_buffer( void *buffer, int *buffer_n, int src_type, struct grn_utrans *transforms, int transforms_n, int *out_err );

enum grn_bencode_operation {
	GRN_BTRANS_DELETE,
	GRN_BTRANS_SET_STRING,
	GRN_BTRANS_SUBSTITUTE,
	GRN_BTRANS_SUBSTITUTE_REGEX,
};

// represents any sort of bulk transform to occur
struct grn_bencode_transform {
	/**
	 * Null-terminated list of strings representing dictionary keys.
	 * It should point to the dictionary object that *contains* what is to be modified
	 * for all operations except for substitute, because substitute works entirely on
	 * the value itself. The key may also just be NULL right away
	 */
	char **key;
	enum grn_bencode_operation operation;
	union grn_transform_payload {
		struct grn_op_delete {
			char *key;
		} delete_;

		struct grn_op_set_string {
			char *key;
			char *val;
		} set_string;

		struct grn_op_substitute {
			char *find;
			char *replace;
		} substitute;

		struct grn_op_substitute_regex {
			// this is inline so we don't have to allocate memory for it and shit
			regex_t find;
			char *replace;
		} substitute_regex;
	} payload;
};

void grn_free_bencode_transform( struct grn_bencode_transform *transform );
// frees the vector too
void grn_free_bencode_transforms_v( struct vector *vec );

/**
* @brief converts a single user transform into a number of bencode transforms and appends them to a vector.
*
* @param vec The vector to append bencode transforms to
* @param utrans A pointer to a single user transform
* @param src_type The type of source we should make the bencode transforms applicable for
* @param btrans_n Will be set to the number of bencode transforms created
*/
void grn_cat_utrans_to_bencode_transforms(struct vector *vec, const struct grn_utrans *utrans, int src_type, int *out_err);

struct grn_run_ctx {
	struct grn_utrans *transforms;
	int transforms_n;
	struct grn_source *srcs;
	int srcs_n;
	int srcs_c;
	int error_c;
	bool done;
	FILE *fh;
	void *buffer;
	size_t buffer_n;
};

struct grn_run_ctx *grn_ctx_alloc( int *out_err );

bool grn_ctx_get_is_done( struct grn_run_ctx *ctx );
// the path of the currently / just processed file
char *grn_ctx_get_c_path( struct grn_run_ctx *ctx );
char *grn_ctx_get_next_path( struct grn_run_ctx *ctx );
int grn_ctx_get_c_error( struct grn_run_ctx *ctx );
// returns the progress as it would be after the current file is done processing.
double grn_ctx_get_progress( struct grn_run_ctx *ctx );

/**
 * Free a context
 * @param ctx a greeny context.
 */
void grn_ctx_free( struct grn_run_ctx *ctx, int *out_err );

// WARNING: if any of the next few context processing functions fail, run grn_ctx_free and abort.
// all errors are fatal.

/**
 * Continue processing a context until the current file is done, but does not proceed to the next file.
 * You can use grn_ctx_get_c_path, etc after calling this.
 * Repeatedly calls grn_one_step internally.
 * @param ctx a grn context
 * @return whether the context is done being processed
 */
bool grn_one_file( struct grn_run_ctx *ctx, int *out_err );

/**
 * Process a context until completion.
 * @param ctx a grn context
 */
void grn_one_context( struct grn_run_ctx *ctx, int *out_err );

#endif

