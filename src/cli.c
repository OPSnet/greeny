#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#include "libannouncebulk.h"
#include "vector.h"

char version_text[] = "GREENY, the Graphical, Really Easy Editor for torreNts, Yup!\n"
	"You are using the GREENY command line interface, version PRE_ALPHA\n"
	"This software's copyright is assigned to the public domain.\n";
char help_text[] = "USAGE:\n"
	"\n"
	"greeny [ OPTIONS ] [ -- ] input_file_1 input_file\n"
	"If no options are specified, --orpheus is assumed. This behavior may be removed in a later version.\n"
	"\n"
	"OPTIONS:\n"
	"\n"
	"  -h               Show this help text.\n"
	"  -v               Show the version.\n"
	"  -t               Specify a custom transform.\n"
	"\n"
	"  --orpheus        Use the preset to transform for Orpheus. This is the default.\n"
	"\n"
	"  --qbittorrent    Transform qbittorrent files in-place.\n"
	"  --qbit           ditto.\n"
	"  --deluge         Ditto for deluge.\n"
	"  --transmission   Ditto for transmission.\n"
	"  --rtorrent       Ditto for rtorrent.\n";

int main(int argc, char **argv) {
	int in_err;

	bool qbittorrent;

	char shortopts[] = "t:hv";
	struct option longopts[] = {
		{ 0 },
	};

	struct vector *files = vector_alloc(&in_err);
	if (in_err) goto cleanup_err;
	struct grn_ctx ctx;
	grn_ctx_alloc(&ctx, &in_err);
	if (in_err) goto cleanup_err;

	int opt_c = 0;
	while (opt_c = getopt_long(argc, argv, shortopts, longopts, NULL) != -1) {
		switch ((char)opt_c) {
			// unknown option
			case '?':;
				goto cleanup_ok;
				break;
			case 't':;
				puts("Not implemented yet.");
				break;
			case 'h':;
				printf(help_text);
				goto cleanup_ok;
				break;
			case 'v':;
				printf(version_text);
				goto cleanup_ok;
				break;
			// other might mean 0, for a long opt. No need to handle it.
		}
	}

	// add normal files
	for (; optind < argc; optind++) {
		printf("Adding %s and subdirectories.\n", argv[optind]);
		grn_cat_torrent_files(files, argv[optind], NULL, &in_err);
		if (in_err) goto cleanup_err;
	}

	int files_n = vector_length(files);
	if (files_n == 0) {
		puts("No files to transform.");
		return 0;
	}
	printf("About to process %d files.\n", files_n);

	// TODO: only proceed if no custom transforms specified.
	if (true) {
		// TODO: add the default arguments
	}

	puts("Transformations complete without error. Thank greeny.");
	return 0;

	cleanup_err:
		puts(grn_err_to_string(in_err));
		vector_free(files);
		grn_ctx_free(&ctx, &in_err);
		return 1;

	cleanup_ok:
		vector_free(files);
		grn_ctx_free(&ctx, &in_err);
		return 0;
};
