#ifndef H_LIBANNOUNCEBULK
#define H_LIBANNOUNCEBULK

#include <stdbool.h>

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
