#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#include "libannouncebulk.h"
#include "vector.h"
#include "err.h"

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

int main( int argc, char **argv ) {
	int in_err;

	// basically bools, but getopt_long wants an int
	int qbittorrent = false;

	char shortopts[] = "t:hv";
	struct option longopts[] = {
		{
			.name = "qbittorrent",
			.has_arg = 0,
			.flag = &qbittorrent,
			.val = true,
		},
		{ 0 },
	};

	struct vector *files = vector_alloc( &in_err );
	if ( in_err ) {
		puts( "Failed to allocate files array." );
		return 1;
	}
	struct vector *transforms = vector_alloc( &in_err );
	if ( in_err ) {
		puts( "Failed to allocate transforms array." );
		return 1;
	}
	struct grn_ctx *ctx = grn_ctx_alloc( &in_err );
	if ( in_err ) goto cleanup_err;

	int opt_c = 0;
	while ( ( opt_c = getopt_long( argc, argv, shortopts, longopts, NULL ) != -1 ) ) {
		switch ( ( char )opt_c ) {
			// unknown option
			case '?':
				;
				goto cleanup_ok;
				break;
			case 't':
				;
				puts( "Not implemented yet." );
				break;
			case 'h':
				;
				puts( help_text );
				goto cleanup_ok;
				break;
			case 'v':
				;
				puts( version_text );
				goto cleanup_ok;
				break;
				// other might mean 0, for a long opt. No need to handle it.
		}
	}

	// add client-specific files
	if ( qbittorrent ) {
		grn_cat_client( files, GRN_CLIENT_QBITTORENT, &in_err );
		if ( in_err ) {
			puts( "Error with qBittorrent." );
			goto cleanup_err;
		}
	}

	// add normal files
	for ( ; optind < argc; optind++ ) {
		printf( "Adding %s and subdirectories.\n", argv[optind] );
		grn_cat_torrent_files( files, argv[optind], NULL, &in_err );
		if ( in_err == GRN_ERR_FS ) {
			printf( "Error adding %s -- file may not exist.", argv[optind] );
		}
		if ( in_err ) {
			goto cleanup_err;
		}
	}

	int files_n = vector_length( files );
	if ( files_n == 0 ) {
		puts( "No files to transform." );
		goto cleanup_ok;
	}
	printf( "About to process %d files.\n", files_n );

	if ( vector_length( transforms ) == 0 ) {
		grn_cat_transforms_orpheus( transforms, &in_err );
		if ( in_err ) {
			goto cleanup_err;
		}
	}

	grn_ctx_set_files_v( ctx, files );
	grn_ctx_set_transforms_v( ctx, transforms, &in_err );
	if ( in_err ) {
		puts( "Error setting transforms." );
		goto cleanup_err;
	}

	// on this blessed day, all files and transforms are in place. Let's do the thing!
	while ( !grn_one_file( ctx, &in_err ) ) {
		if ( in_err ) {
			printf( "Error during transforming: %s\n", grn_err_to_string( in_err ) );
			goto cleanup_err;
		} else {
			// TODO: file name
			puts( "File transformed successfully." );
		}
	}

	puts( "Transformations complete without error. Thank greeny." );
	goto cleanup_ok;

cleanup_err:
	puts( grn_err_to_string( in_err ) );
	vector_free_all( files );
	vector_free_all( transforms );
	grn_ctx_free( ctx, &in_err );
	return 1;

cleanup_ok:
	vector_free_all( files );
	vector_free_all( transforms );
	grn_ctx_free( ctx, &in_err );
	return 0;
}
