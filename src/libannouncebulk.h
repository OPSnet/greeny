#ifndef H_LIBANNOUNCEBULK
#define H_LIBANNOUNCEBULK

#include <stdbool.h>

enum anb_error {
	ANB_OK = 0,
	ANB_ERR_OOM,
	ANB_ERR_FS,
	ANB_ERR_WRONG_BENCODE_TYPE,
	ANB_ERR_WRONG_TRANSFORM_OPERATION,
	// represents BEN errors 1, 2, 4. Ben error 0 is ANB_OK, 3 is ANB_ERR_OOM
	ANB_ERR_BENCODE_SYNTAX,
};

enum anb_error ben_error_to_anb(int bencode_error);
char *anb_error_to_human(enum anb_error *out_err);

enum anb_operation {
	ANB_TRANSFORM_DELETE,
	ANB_TRANSFORM_SET_STRING,
	ANB_TRANSFORM_SUBSTITUTE,
};

// represents any sort of bulk transform to occur
struct anb_transform {
	/**
	 * Null-terminated list of strings representing dictionary keys.
	 * It should point to the dictionary object that *contains* what is to be modified
	 * for all operations except for substitute, because substitute works entirely on
	 * the value itself. The key may also just be NULL right away
	 */
	char **key;
	enum anb_operation operation;
	union anb_transform_payload {
		struct anb_op_delete {
			char *key;
		} delete_;

		struct anb_op_set_string {
			char *key;
			char *val;
		} set_string;

		struct anb_op_set_bool {
			char *key;
			bool val;
		} set_bool;

		struct anb_op_substitute {
			char *find;
			char *replace;
		} substitute;
	} payload;
};

void anb_transform_free(struct anb_transform *transform, enum anb_error *out_err);

struct anb_callback_arg {
	// progress bar info
	int numerator;
	int denominator;
	char *prev_path;
	char *next_path;
};

struct anb_transform_result {
	char *path;
	char error[64];
};

struct anb_main_arg {
	// bool recurse;
	char **paths;
	int paths_n;
	// bool deluge;
	// bool transmission;
	// bool qbittorrent;
	// bool rtorrent;
	struct anb_transform *transforms;
	int transforms_n;
	void *callback_ctx;
	int (*callback)(struct anb_callback_arg callback_arg, void *ctx);
};

typedef struct {
	char *path;
	bool directory;
} anb_user_path;

typedef struct {
	char *path;
	char *buffer;
	int buffer_n;
} anb_file;

/**
 * @param main_args information gathered from a cli or gui
 * @return 0 on fail, nonzero on success
 */
int anb_main(struct anb_main_arg args);

/** 
 * @param transforms list of transforms to perform
 * @param transforms_n number of transforms to perform
 * @param buffer a pointer to the pointer to the buffer to perform the transform on, and which will be
 * set to the modified buffer. The buffer passed in might be free()-ed, memcpy if you care about it.
 * @param buffer_n the length of the buffer, will be updated if modified
 */
void anb_transform_buffer(struct anb_transform *transforms, int transforms_n, char **buffer, size_t *buffer_n, enum anb_error *out_err);

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
char **anb_find_torrents(bool recurse, anb_user_path *in_paths, int in_paths_n, int *out_paths_n);

/**
 * @param path the path to the torrent file on-disk
 * @return the file with contents read
 */
anb_file anb_read_file(char* path);

/**
 * @param file the file to write to disk
 * @return whether the writing was sucessful
 */
bool anb_write_file(anb_file file);

/**
 * @param file the file to free
 */
void anb_free_file(anb_file file);

/**
 * @param from the text to remove
 * @param to the text to replace the removed text with
 * @param file the file to perform the replacement on. Does not write to disk, only modifies buffer
 */
void anb_replace_announce_buffer(char *from, char *to, anb_file file);

/**
 * @param to what to set the announce to
 * @param file file to perform set inside of
 * If currently a list, it will be set to a string.
 */
void anb_set_announce_buffer(char *to, anb_file file);

#endif
