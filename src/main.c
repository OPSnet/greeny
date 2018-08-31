#include <stdio.h>
#include <stdbool.h>

#include "libannouncebulk.h"

int main() {
	int paths_n;
	anb_user_path in_path;
	in_path.path = "/tmp";
	in_path.directory = true;
	anb_user_path in_path_2;
	in_path_2.path = "/home/markasoftware/Downloads";
	in_path_2.directory = true;
	anb_user_path in_paths[2] = {in_path, in_path_2};
	char **paths = anb_find_torrents(false, &in_paths, 2, &paths_n);
	printf("%d\n", paths_n);
	for (int i = 0; i < paths_n; i++) {
		puts(paths[i]);
	}
}
