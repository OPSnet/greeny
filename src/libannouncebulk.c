#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include "libannouncebulk.h"
// are you supposed to do this?
#include "../contrib/bencode.h"

// BEGIN vector implementation
typedef struct {
	void **buffer;
	int used_n;
	int allocated_n;
} vector;

static vector *vector_alloc() {
	vector *to_return;
	if (!(to_return = malloc(sizeof(vector)))) {
		return NULL;
	}
	if (!(to_return->buffer = malloc(sizeof(void *)))) {
		return NULL;
	}
	to_return->used_n = 0;
	to_return->allocated_n = 1;
	return to_return;
}

static void vector_free(vector *free_me) {
	free(free_me->buffer);
	free(free_me);
}

static int vector_push(void *push_me, vector *vector) {
	vector->used_n++;
	if (vector->used_n > vector->allocated_n) {
		vector->allocated_n *= 2;
		if (!(vector->buffer = realloc(vector->buffer, vector->allocated_n * sizeof(void*)))) {
			return 0;
		}
	}
	vector->buffer[vector->used_n - 1] = push_me;
	return 1;
}

static void **vector_export(vector *vector, int *n) {
	*n = vector->used_n;
	void **to_return;
	if (!(to_return = realloc(vector->buffer, vector->used_n * sizeof(void*)))) {
		return NULL;
	}
	free(vector);
	return to_return;
}

// END vector implementation

// BEGIN filesystem helpers

/**
 * @param fh file handle from fopen
 * @param out_n where length of the file will be placed
 * @return buffer with file contents, or NULL on error
 */
static char *read_file(FILE *fh, size_t *out_n) {
	// we can't just do fread(buffer, 1, some_massive_num, fh) because we can't be sure whether
	// the whole file was read or not.
	if (fseek(fh, 0, SEEK_END)) return NULL;
	*out_n = ftell(fh);
	if (fseek(fh, 0, SEEK_SET)) return NULL;
	void *buffer = malloc(*out_n);
	if (fread(buffer, *out_n, 1, fh) != 1) {
		free(buffer);
		return NULL;
	}
	return buffer;
};

/**
 * @param fh the file handle from fopen
 * @param buffer the data to write
 * @param buffer_n length of data to write
 * @return 0 on success, nonzero on failure
 */
static int write_file(FILE *fh, void *buffer, size_t buffer_n) {
	if (fwrite(buffer, buffer_n, 1, fh) != 1) return -1;
	return 0;
}

// END filesystem helpers

// assumes enough space is in haystack to avoid heap allocations.
static int strsubst(char *haystack, char *find, char *replace) {
	// no need to be fast about it.
	int haystack_n = strlen(haystack), find_n = strlen(find), replace_n = strlen(replace);

	char *needle_start = strstr(find, replace);
	// not contained
	if (needle_start == NULL) return 0;
	int prefix_n = haystack_n - strlen(needle_start);
	int suffix_n = haystack_n - prefix_n - find_n;

	char *temp;
	// simpler than MAX
	if (!(temp = malloc(haystack_n + find_n + replace_n + 1))) return 1;
	temp[0] = '\0';
	strncat(haystack, temp, prefix_n);

	// copy substitution
	// intentionally ignore null byte on replace
	strcat(replace, temp + prefix_n);

	// copy suffix
	strcat(needle_start + find_n, temp + prefix_n + replace_n);

	strcpy(temp, haystack);
	free(temp);
	return 0;
}

// just make sure never to update bencode.c lol
static int ben_str_subst(bencode *haystack, char *find, char *replace) {
	if (haystack->type != BENCODE_STR) return 1;
	if (!(haystack->s = realloc(haystack->s, haystack->len + strlen(find) + strlen(replace) + 1))) return 1;
	if (strsubst(haystack->s, find, replace)) return 1;
	haystack->len = strlen(haystack->s);
	return 0;
}

static int str_ends_with(char *haystack, char *needle) {
	int haystack_n = strlen(haystack);
	int needle_n = strlen(needle);
	char *haystack_suffix = haystack + haystack_n - needle_n;
	return strcmp(haystack_suffix, needle);
}

char **anb_find_torrents(bool recurse, anb_user_path *in_paths, int in_paths_n, int *out_paths_n) {
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

// this will fail if there are null bytes in the string, which shouldn't be the case for any human readable ones.
// TODO: make work with null bytes in strings.
static int ben_substitute_value(char *key, char *find, char *replace, bencode *dict) {
	bencode *val = ben_dict_get(dict, key);
	if (val->type == BENCODE_STR) {
		ben_str_subst(val, find, replace);
	} else if (val->type == BENCODE_LIST) {
		size_t list_n = ben_list_len(val);
		for (int i = 0; i < list_n; i++) {
			bencode *list_cur = ben_list_get(val, i);
			if (list_cur->type != BENCODE_STR) return 1;
			ben_str_subst(list_cur, find, replace);
		}
	} else {
		// TODO: handle it if not string nor list. Probably best option is to use an actual enum of error
		// value so that we can detect whether there was a real problem (OOM) or just invalid type from parent.
		return 1;
	}
}

int anb_transform_buffer(anb_transform *transforms, int transforms_n, char **buffer, int *buffer_n) {
	int bencode_error;
	struct bencode *main_dict = ben_decode2(*buffer, *buffer_n, NULL, &bencode_error);
	if (bencode_error != BEN_OK) return 1;
	if (main_dict->type != BENCODE_DICT) return 1;

	for (int i = 0; i < transforms_n; i++) {
		// TODO: it may be faster to loop through all keys then check if a transform applies. It's my
		// uneducated guess that the ben_dict_{get,set} do an iterative search for the key anyway.
		anb_transform transform = transforms[i];
		switch (transform.operation) {
			case ANB_TRANSFORM_DELETE:
				// it returns a "standalone" pointer to the value, that must be freed. It modifies
				// the main_dict in place
				ben_free(ben_dict_pop(main_dict, transform.key));
				break;
			case ANB_TRANSFORM_SET:
				// TODO: maybe support setting an array?
				bencode *key = ben_str(key);
				bencode *val = ben_str(transform.payload.set);
				if (ben_dict_set(main_dict, key, val)) return 1;
				break;
			case ANB_TRANSFORM_REPLACE:
				if (ben_substitute_value(transform.key, transform.payload.subst.find, transform.payload.subst.replace, main_dict)) return 1;
				break;
			default:
				return 1;
		}
	}

	// TODO: consider checking whether the file was actually modified to do this lazily?
	free(*buffer);
	if(!(*buffer = ben_encode(buffer_n, main_dict))) return 1;
	ben_free(main_dict);
	return 0;
}

int anb_main(anb_main_arg arg) {
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
		if (anb_transform_buffer(arg.transforms, arg.transforms_n, &buffer, &buffer_n)) {
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
