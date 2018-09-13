#ifndef H_LIBANNOUNCEBULK
#define H_LIBANNOUNCEBULK

#include <stdbool.h>
#include "vector.h"

int ben_error_to_anb(int bencode_error);
char *grn_error_to_human(int *out_err);

enum grn_operation {
	GRN_TRANSFORM_DELETE,
	GRN_TRANSFORM_SET_STRING,
	GRN_TRANSFORM_SUBSTITUTE,
};
enum grn_operation grn_human_to_operation(char *human, int *out_err);

// represents any sort of bulk transform to occur
struct grn_transform {
	/**
	 * Null-terminated list of strings representing dictionary keys.
	 * It should point to the dictionary object that *contains* what is to be modified
	 * for all operations except for substitute, because substitute works entirely on
	 * the value itself. The key may also just be NULL right away
	 */
	char **key;
	enum grn_operation operation;
	union grn_transform_payload {
		struct grn_op_delete {
			char *key;
		} delete_;

		struct grn_op_set_string {
			char *key;
			char *val;
		} set_string;

		struct grn_op_set_bool {
			char *key;
			bool val;
		} set_bool;

		struct grn_op_substitute {
			char *find;
			char *replace;
		} substitute;
	} payload;
};

void grn_transform_free(struct grn_transform *transform, int *out_err);

struct grn_callback_arg {
	// progress bar info
	int numerator;
	int denominator;
	char *prev_path;
	char *next_path;
};

struct grn_transform_result {
	char *path;
	char error[64];
};

struct grn_main_arg {
	// bool recurse;
	// NULL terminated
	char **paths;
	// bool deluge;
	// bool transmission;
	// bool qbittorrent;
	// bool rtorrent;
	// NULL terminated
	struct grn_transform *transforms;
	void *callback_ctx;
	int (*callback)(struct grn_callback_arg callback_arg, void *ctx);
};

typedef struct {
	char *path;
	bool directory;
} grn_user_path;

typedef struct {
	char *path;
	char *buffer;
	int buffer_n;
} grn_file;

/**
 * @param main_args information gathered from a cli or gui
 * @return 0 on fail, nonzero on success
 */
int grn_main(struct grn_main_arg args);

/** 
 * @param transforms list of transforms to perform
 * @param transforms_n number of transforms to perform
 * @param buffer a pointer to the pointer to the buffer to perform the transform on, and which will be
 * set to the modified buffer. The buffer passed in might be free()-ed, memcpy if you care about it.
 * @param buffer_n the length of the buffer, will be updated if modified
 */
void grn_transform_buffer(struct grn_transform *transforms, int transforms_n, char **buffer, size_t *buffer_n, int *out_err);

/**
 * @param recurse whether to recurse into subdirectories. Will go one level by default.
 * @param in_paths array of paths to read from
 * @param in_paths_n number of paths to read from
 * @param out_paths_n number of paths outputted.
 * @return array of fully qualified file paths. We allocate it, free with `free`
 * Possible modifications:
 *  - Why not use a NULL-terminated array of arrays?
 *  - Custom recursion level
 */
char **grn_find_torrents(bool recurse, grn_user_path *in_paths, int in_paths_n, int *out_paths_n);

/**
 * @param path the path to the torrent file on-disk
 * @return the file with contents read
 */
grn_file grn_read_file(char* path);

/**
 * @param file the file to write to disk
 * @return whether the writing was sucessful
 */
bool grn_write_file(grn_file file);

/**
 * @param file the file to free
 */
void grn_free_file(grn_file file);

/**
 * @param from the text to remove
 * @param to the text to replace the removed text with
 * @param file the file to perform the replacement on. Does not write to disk, only modifies buffer
 */
void grn_replace_announce_buffer(char *from, char *to, grn_file file);

/**
 * @param to what to set the announce to
 * @param file file to perform set inside of
 * If currently a list, it will be set to a string.
 */
void grn_set_announce_buffer(char *to, grn_file file);

// BEGIN preset and semi-preset transforms

/**
 * @param find text to find
 * @param replace text to replace found text with
 * @return a NULL-terminated array of pointers to transforms
 */
struct grn_transform *grn_new_announce_substitution_transform(const char *find, const char *replace);
struct grn_transform *grn_orpheus_transforms;

// END

#endif
