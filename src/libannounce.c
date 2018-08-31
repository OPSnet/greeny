#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
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
	vector *to_return = malloc(sizeof(vector));
	to_return->buffer = malloc(sizeof(void *));
	to_return->used_n = 0;
	to_return->allocated_n = 1;
	return to_return;
}

static void vector_free(vector *free_me) {
	free(free_me->buffer);
	free(free_me);
}

static void vector_push(void *push_me, vector *vector) {
	vector->used_n++;
	if (vector->used_n > vector->allocated_n) {
		vector->allocated_n *= 2;
		vector->buffer = realloc(vector->buffer, vector->allocated_n * sizeof(void*));
	}
	vector->buffer[vector->used_n - 1] = push_me;
}

static void **vector_export(vector *vector, int *n) {
	*n = vector->used_n;
	void **to_return = realloc(vector->buffer, vector->used_n * sizeof(void*));
	free(vector);
	return to_return;
}

// END vector implementation

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
