#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include "libannouncebulk.h"
#include "vector.h"
#include "err.h"
// are you supposed to do this?
#include "../contrib/bencode.h"

// BEGIN macros


// END macros

// BEGIN vector implementation
typedef struct {
	void **buffer;
	int used_n;
	int allocated_n;
} vector;


// END vector implementation

// BEGIN filesystem helpers

/**
 * @param fh file handle from fopen
 * @param out_n where length of the file will be placed
 * @return buffer with file contents, or NULL on error
 */
static char *read_file(FILE *fh, size_t *out_n, int *out_err) {
	// we can't just do fread(buffer, 1, some_massive_num, fh) because we can't be sure whether
	// the whole file was read or not.
	ERR_NULL(fseek(fh, 0, SEEK_END), GRN_ERR_FS);
	*out_n = ftell(fh);
	ERR_NULL(fseek(fh, 0, SEEK_SET), GRN_ERR_FS);
	void *buffer = malloc(*out_n);
	ERR_NULL(!buffer, GRN_ERR_OOM);
	if (fread(buffer, *out_n, 1, fh) != 1) {
		free(buffer);
		ERR_NULL(GRN_ERR_FS);
	}
	RETURN_OK(buffer);
};

/**
 * @param fh the file handle from fopen
 * @param buffer the data to write
 * @param buffer_n length of data to write
 * @return 0 on success, nonzero on failure
 */
static void write_file(FILE *fh, void *buffer, size_t buffer_n, int *out_err) {
	ERR(fwrite(buffer, buffer_n, 1, fh) != 1, GRN_ERR_FS);
	RETURN_OK();
}

// END filesystem helpers

// BEGIN custom data type operations

char *grn_error_to_string(int error) {
	static char *err_strings[] = {
		"Successful.",
		"Out of memory.",
		"Filesystem error.",
		"Wrong bencode type (eg, int where string expected).",
		"Wrong transform operation (unknown).",
		"Invalid bencode syntax.",
	};
	return err_strings[error];
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
	{
		announce_list_key,
		GRN_TRANSFORM_SUBSTITUTE,
		"apollo.rip",
		"orpheus.network",
	},
	NULL,
};

struct grn_transform *grn_new_announce_substitution_transform(const char *find, const char *replace) {
	return {
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

static int str_ends_with(char *haystack, char *needle) {
	int haystack_n = strlen(haystack);
	int needle_n = strlen(needle);
	char *haystack_suffix = haystack + haystack_n - needle_n;
	return strcmp(haystack_suffix, needle);
}

/*
char **grn_find_torrents(bool recurse, grn_user_path *in_paths, int in_paths_n, int *out_paths_n) {
	vector *out_paths = vector_alloc();
	for (int i = 0; i < in_paths_n; i++) {
		if (in_paths[i].directory) {
			DIR *dir = opendir(in_paths[i].path);
			struct dirent *dir_entry;
			while (dir_entry = readdir(dir)) {
				// ignore other thinks for now
				if (
					(dir_entry->d_type == DT_REG || dir_entry->d_type == DT_LNK) &&
					str_ends_with(dir_entry->d_name, ".torrent") == 0
				) {
					vector_push(dir_entry->d_name, out_paths);
				}
			}
		} else {
			// simple file
			vector_push(in_paths[i].path, out_paths);
		}
	}
	return vector_export(out_paths, out_paths_n);
}
*/

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

void grn_transform_buffer(struct grn_transform *transforms, int transforms_n, char **buffer, size_t *buffer_n, int *out_err) {
	int bencode_error;
	size_t off = 0;
	struct bencode *main_dict = ben_decode2(*buffer, *buffer_n, &off, &bencode_error);
	ERR(bencode_error, bencode_error_to_anb(bencode_error));

	for (int i = 0; i < transforms_n; i++) {
		struct grn_transform transform = transforms[i];

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
	free(*buffer);
	*buffer = ben_encode(buffer_n, main_dict);
	ERR(!buffer, GRN_ERR_OOM);
	ben_free(main_dict);
	RETURN_OK();
}

/*
int grn_main(struct grn_main_arg arg) {
	// Find all the file paths we will be using: todo.
	char **paths = arg.paths;
	int paths_n = arg.paths_n;

	// loop for each file
	for (int i = 0; i < paths_n; i++) {
		char *path = paths[i];
		FILE *fh;
		if (!(fh = fopen(path, "r"))) {
			printf("WARNING: Could not open %s for reading.\n", path);
			continue;
		}

		char *buffer;
		int buffer_n;

		if (!(buffer = read_file(fh, &buffer_n)) {
			printf("WARNING: Could not read %s\n", path);
			continue;
		}
		// unlike filesystem I/O stuff, transforming the buffer should never fail.
		if (grn_transform_buffer(arg.transforms, arg.transforms_n, &buffer, &buffer_n)) {
			printf("ERROR: Transformation error while processing %s.", path);
			return 0;
		}

		if (!(fh = freopen(NULL, "w", fh))) {
			printf("WARNING: Could not open %s for writing.", path);
			continue;
		}
		if (write_file(fh, buffer)) {
			printf("WARNING: Could not write %s", path);
			continue;
		}
	}

	return 1;
}
*/
