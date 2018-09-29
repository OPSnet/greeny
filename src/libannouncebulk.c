#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <ftw.h>

#include "libannouncebulk.h"
#include "vector.h"
#include "err.h"
// are you supposed to do this?
#include "../contrib/bencode.h"

// BEGIN macros


// END macros

// BEGIN context filesystem

static void fread_ctx(struct grn_ctx *ctx, int *out_err) {
	*out_err = GRN_OK;

	// we can't just do fread(buffer, 1, some_massive_num, fh) because we can't be sure whether
	// the whole file was read or not.
	ERR(fseek(ctx->fh, 0, SEEK_END), GRN_ERR_FS);
	ctx->buffer_n = ftell(ctx->fh);
	ERR(fseek(ctx->fh, 0, SEEK_SET), GRN_ERR_FS);

	ctx->buffer = malloc(ctx->buffer_n);
	ERR(ctx->buffer == NULL, GRN_ERR_OOM);
	if (fread(ctx->buffer, ctx->buffer_n, 1, ctx->fh) != 1) {
		free(ctx->buffer);
		ERR(GRN_ERR_FS);
	}
};

static void fwrite_ctx(struct grn_ctx *ctx, int *out_err) {
	*out_err = GRN_OK;

	ERR(fwrite(ctx->buffer, ctx->buffer_n, 1, ctx->fh) != 1, GRN_ERR_FS);
}

// END context filesystem

// BEGIN custom data type operations

void grn_ctx_alloc(struct grn_ctx *ctx, int *out_err) {
	memset(ctx, 0, sizeof(*ctx));
	ctx->files_v = vector_alloc(out_err);
	ERR_FW();
	RETURN_OK();
}

void grn_ctx_seal(struct grn_ctx *ctx, int *out_err) {
	// null terminate that shit.
	vector_push(ctx->files_v, NULL, out_err);
	ERR_FW();
	ctx->files_v = NULL;
	int vector_n = 0;
	ctx->files = vector_export(ctx->files_v, &vector_n, out_err);
	ERR_FW();
	ctx->files_c = ctx->files;
	ctx->state = GRN_CTX_READ;
	RETURN_OK();
}

void grn_ctx_free(struct grn_ctx *ctx, int *out_err) {
	if (ctx->state == GRN_CTX_UNSEALED) {
		vector_free(ctx->files_v);
		RETURN_OK();
	}
	free(ctx->files);
	if (ctx->buffer != NULL) {
		free(ctx->buffer);
	}
	if (ctx->fh != NULL) {
		ERR(fclose(ctx->fh), GRN_ERR_FS);
	}
	RETURN_OK();
}

char *grn_err_to_string(int err) {
	static char *err_strings[] = {
		"Successful.",
		"Out of memory.",
		"Filesystem error.",
		"Wrong bencode type (eg, int where string expected).",
		"Wrong transform operation (unknown).",
		"Invalid bencode syntax.",
	};
	return err_strings[err];
}


int bencode_error_to_anb(int bencode_error) {
	if (bencode_error == BEN_OK) return GRN_OK;
	if (bencode_error == BEN_NO_MEMORY) return GRN_ERR_OOM;
	return GRN_ERR_BENCODE_SYNTAX;
}

// assumes that all keys and payload strings were dynamically allocated.
void grn_transform_free(struct grn_transform *transform, int *out_err) {
	// free all keys
	char *key_cur;
	int i = 0;
	while ((key_cur = transform->key[i++])) {
		free(key_cur);
	}
	free(transform->key);
	// free payload
	switch (transform->operation) {
		case GRN_TRANSFORM_DELETE:
			free(transform->payload.delete_.key);
			break;
		case GRN_TRANSFORM_SET_STRING:
			free(transform->payload.set_string.key);
			free(transform->payload.set_string.val);
			break;
		case GRN_TRANSFORM_SUBSTITUTE:
			free(transform->payload.substitute.find);
			free(transform->payload.substitute.replace);
			break;
		default:
			ERR(GRN_ERR_WRONG_TRANSFORM_OPERATION);
			break;
	}
}

// END custom data type operations

// BEGIN preset and semi-presets


static char *announce_str_key[] = {
	"announce",
	NULL,
};
static char *announce_list_key[] = {
	"announce_list",
	NULL,
};
struct grn_transform grn_orpheus_transforms[] = {
	{
		announce_str_key,
		GRN_TRANSFORM_SUBSTITUTE,
		"apollo.rip",
		"orpheus.network",
	},
	/*{
		announce_list_key,
		GRN_TRANSFORM_SUBSTITUTE,
		"apollo.rip",
		"orpheus.network",
	},
	NULL,*/
};

struct grn_transform *grn_new_announce_substitution_transform(const char *find, const char *replace) {
	struct grn_transform to_return[] = {
		{
			announce_str_key,
			GRN_TRANSFORM_SUBSTITUTE,
			find,
			replace,
		},
		{
			announce_list_key,
			GRN_TRANSFORM_SUBSTITUTE,
			find,
			replace,
		},
	};
}

// END preset and semi-presets

// returns dynamically allocated
static char *strsubst(const char *haystack, const char *find, const char *replace, int *out_err) {
	char *to_return;

	// no need to be fast about it.
	int haystack_n = strlen(haystack), find_n = strlen(find), replace_n = strlen(replace);

	char *needle_start = strstr(find, replace);
	// not contained
	if (needle_start == NULL) {
		to_return = malloc(strlen(haystack) + 1);
		ERR_NULL(!to_return, GRN_ERR_OOM);
		strcpy(to_return, haystack);
		RETURN_OK(to_return);
	}
	int prefix_n = haystack_n - strlen(needle_start);

	// simpler than MAX
	to_return = malloc(haystack_n + find_n + replace_n + 1);
	ERR_NULL(!to_return, GRN_ERR_OOM);
	to_return[0] = '\0';
	strncat(to_return, haystack, prefix_n);

	// copy substitution
	// intentionally ignore null byte on replace
	strcat(to_return, replace);

	// copy suffix
	strcat(to_return, haystack + prefix_n + replace_n);

	RETURN_OK(to_return);
}

static void ben_str_subst(struct bencode *haystack, char *find, char *replace, int *out_err) {
	ERR(haystack->type != BENCODE_STR, GRN_ERR_WRONG_BENCODE_TYPE);
	char *substituted = strsubst(ben_str_val(haystack), find, replace, out_err);
	ERR_FW();

	struct bencode_str *haystack_str = (struct bencode_str *)haystack;
	haystack_str->s = substituted;
	haystack_str->len = strlen(substituted);
	RETURN_OK();
}

/**
 * @param haystack the string that should end with needle
 * @param needle the string haystack should end with
 * @return true if haystack ends with needle
 */
static bool str_ends_with(const char *haystack, const char *needle) {
	int haystack_n = strlen(haystack);
	int needle_n = strlen(needle);
	const char *haystack_suffix = haystack + haystack_n - needle_n;
	return strcmp(haystack_suffix, needle) == 0;
}

// this will have undefined behavior if there are null bytes in the string
// let's be honest though, it will just truncate it after the first null byte
static void ben_substitute(struct bencode *ben, char *find, char *replace, int *out_err) {
	if (ben->type == BENCODE_STR) {
		ben_str_subst(ben, find, replace, out_err);
		ERR_FW();
	} else if (ben->type == BENCODE_LIST) {
		size_t list_n = ben_list_len(ben);
		for (int i = 0; i < list_n; i++) {
			struct bencode *list_cur = ben_list_get(ben, i);
			ERR(list_cur->type != BENCODE_STR, GRN_ERR_WRONG_BENCODE_TYPE);
			ben_str_subst(list_cur, find, replace, out_err);
			ERR_FW();
		}
	} else {
		ERR(GRN_ERR_WRONG_BENCODE_TYPE);
	}
	RETURN_OK();
}

void transform_buffer(struct grn_ctx *ctx, int *out_err) {
	*out_err = GRN_OK;

	int bencode_error;
	size_t off = 0;
	struct bencode *main_dict = ben_decode2(ctx->buffer, ctx->buffer_n, &off, &bencode_error);
	ERR(bencode_error, bencode_error_to_anb(bencode_error));

	for (int i = 0; i < ctx->transforms_n; i++) {
		struct grn_transform transform = ctx->transforms[i];

		// first, filter down by the keys in the transform
		struct bencode *filtered = main_dict;
		char *filter_key;
		int k = 0;
		while ((filter_key = transform.key[k++])) {
			ERR(filtered->type != BENCODE_DICT, GRN_ERR_WRONG_BENCODE_TYPE);
			filtered = ben_dict_get_by_str(filtered, filter_key);
		}

		switch (transform.operation) {
			case GRN_TRANSFORM_DELETE:;
				// it returns a "standalone" pointer to the value, that must be freed. It modifies
				// the main_dict in place
				ben_free(ben_dict_pop_by_str(filtered, transform.payload.delete_.key));
				break;
			case GRN_TRANSFORM_SET_STRING:;
				// TODO: maybe support setting an array?
				struct grn_op_set_string setstr_payload = transform.payload.set_string;
				ERR(ben_dict_set_str_by_str(filtered, setstr_payload.key, setstr_payload.val), GRN_ERR_OOM);
				break;
			case GRN_TRANSFORM_SUBSTITUTE:;
				struct grn_op_substitute subst_payload = transform.payload.substitute;
				ben_substitute(filtered, subst_payload.find, subst_payload.replace, out_err);
				ERR_FW();
				break;
			default:;
				ERR(GRN_ERR_WRONG_TRANSFORM_OPERATION);
				break;
		}
	}

	// TODO: consider checking whether the file was actually modified to do this lazily?
	free(ctx->buffer);
	ctx->buffer = ben_encode(ctx->buffer_n, main_dict);
	ERR(!ctx->buffer, GRN_ERR_OOM);
	ben_free(main_dict);
}

/**
 * Truncates the file and opens in writing mode.
 */
static void freopen_ctx(struct grn_ctx *ctx, int *out_err) {
	*out_err = GRN_OK;
	// it will get fclosed by the caller with grn_ctx_free
	ERR(freopen(NULL, ctx->fh, "w"), GRN_ERR_FS);
}

static void next_file_ctx(struct grn_ctx *ctx, int *out_err) {
	*out_err = GRN_OK;

	// close the previously processing file
	if (ctx->fh) {
		ERR(fclose(ctx->fh), GRN_ERR_FS);
	}

	ctx->files_c++;
	// are we done?
	if (ctx->files_c >= ctx->files_n) {
		ctx->state = GRN_CTX_DONE;
		return;
	}

	// prepare the next file for reading
	ERR(ctx->fh = fopen(ctx->files[ctx->files_c], "r"), GRN_ERR_FS);
}

// BEGIN mainish functions

bool grn_one_step(struct grn_ctx *ctx, int *out_err) {
	*out_err = GRN_OK;
	switch (ctx->state) {
		case GRN_CTX_DONE:;
			return true;
			break;
		case GRN_CTX_UNSEALED:;
			*out_err = GRN_ERR_WRONG_CTX_STATE;
			return false;
			break;
		case GRN_CTX_READ:;
			// means we should continue reading the current file
			// fread_ctx will only "throw" an error if it's not an FS problem (which indicates file specific problem).
			// TODO: consider and maybe actually do what is described just above
			fread_ctx(ctx, out_err);
			// fuckin macros
			if (*out_err) {
				// mostly just to be safe, since we don't reassign later it would probably be ok without this.
				return false;
			}
			ctx->state = GRN_CTX_TRANSFORM;
			return false;
			// TODO: once we add non-blocking, call grn_one_step again here
			break;
		case GRN_CTX_WRITE:;
			bool more_writing = fwrite_ctx(ctx, out_err);
			if (*out_err) {
				return false;
			}
			next_file_ctx(ctx, out_err);
			if (*out_err) {
				return false;
			}
			if (ctx->state == GRN_CTX_DONE) {
				return true;
			}
			// TODO: call grn_one_step
			return false;
			break;
		case GRN_CTX_TRANSFORM:;
			transform_buffer(ctx, out_err);
			ctx->state = GRN_CTX_WRITE;
			// TODO: run grn_one_step again
			return false;
			break;
	}
}

bool grn_one_file(struct grn_ctx *ctx, int *out_err) {
	*out_err = GRN_OK;

	int files_c_old = ctx->files_c;
	while (ctx->files_c == files_c_old && ctx->state != GRN_CTX_DONE) {
		grn_one_step(ctx, out_err);
		if (out_err) {
			return false;
		}
	}
	return ctx->state == GRN_CTX_DONE;
}

void grn_one_ctx(struct grn_ctx *ctx, int *out_err) {
	*out_err = GRN_OK;

	while (ctx->state != GRN_CTX_DONE) {
		grn_one_step(ctx, out_err);
		if (out_err) {
			return false;
		}
	}
}

// END mainish functions


// global because nftw doesn't support a custom callback argument
static struct vector *cat_vec;
static const char *cat_ext;

// used as nftw callback below
static int cat_nftw_cb(const char *path, const struct stat *st, int file_type, struct FTW *ftw_info) {
	int in_err;

	// ignore non-files and files without the correct extension
	if (
		file_type != FTW_F ||
		!str_ends_with(path, cat_ext)
	) {
		return 0;
	}

	vector_push(cat_vec, path, &in_err);
	return in_err;
}

void grn_cat_torrent_files(struct vector *vec, const char *path, const char *extension, int *out_err) {
	*out_err = GRN_OK;

	cat_vec = vec;
	cat_ext = extension != NULL ? extension : ".torrent";

	int nftw_err = nftw(path, cat_nftw_cb, 16, 0);
	if (nftw_err) {
		// if nftw returns -1, it means there was some internal problem. Any other error means that our cb failed.
		*out_err = nftw_err == -1 ? GRN_ERR_FS : nftw_err;
		return;
	}
}
