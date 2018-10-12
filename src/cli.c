#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#include "libannouncebulk.h"
#include "vector.h"
#include "err.h"

struct cli_ctx {
	struct vector *transforms;
	struct vector *files;

	char *orpheus_user_announce;

#define X_CLIENT(x_machine, x_enum, x_human) int x_machine;
#include "x_clients.h"
#undef X_CLIENT

	struct grn_ctx *grn_ctx;
};

static void die_silent( struct cli_ctx *cli_ctx );
static void die_if( struct cli_ctx *cli_ctx, int err );
static void die_if_fatal( struct cli_ctx *cli_ctx, int err );
static void exit_kindly( struct cli_ctx *cli_ctx );

// allocate vectors
static void cli_ctx_alloc( struct cli_ctx *cli_ctx );
static void cli_ctx_free_cats( struct cli_ctx *cli_ctx );
static void cli_ctx_free( struct cli_ctx *cli_ctx );

// uses getopt_long to parse CLI options. Will add transforms.
static void handle_opts( struct cli_ctx *cli_ctx, int *argind, int argc, char **argv );
static void cat_transforms( struct cli_ctx *cli_ctx );
// uses the mutilated argv from getopt_long which only has files in it now
// argind is optind
static void cat_files( struct cli_ctx *cli_ctx, int argind, int argc, char **argv );

static void seal( struct cli_ctx *cli_ctx );

static void main_loop( struct cli_ctx *cli_ctx );

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
                   "CLIENTS:"
                   "Pass these arguments to modify the files for a certain BitTorrent client. You may need to restart it after running GREENY.\n"
#define X_CLIENT(x_machine, x_enum, x_human) "  --" #x_machine ": " x_human "\n"
#include "x_clients.h"
#undef X_CLIENT
                   "";

int main( int argc, char **argv ) {
	struct cli_ctx cli_ctx;

	cli_ctx_alloc( &cli_ctx );

	int argind;
	handle_opts( &cli_ctx, &argind, argc, argv );
	cat_transforms( &cli_ctx );
	cat_files( &cli_ctx, argind, argc, argv );

	seal( &cli_ctx );
	main_loop( &cli_ctx );

	exit_kindly( &cli_ctx );
}

static void die_silent( struct cli_ctx *cli_ctx ) {
	cli_ctx_free( cli_ctx );
	exit( EXIT_FAILURE );
}

static void die_if( struct cli_ctx *cli_ctx, int err ) {
	if ( err ) {
		printf( "ERROR: %s\nGreeny will now exit prematurely.", grn_err_to_string( err ) );
		die_silent( cli_ctx );
	}
}

static void die_if_fatal( struct cli_ctx *cli_ctx, int err ) {
	if ( grn_err_is_fatal( err ) ) {
		printf( "not implemented" );
		die_silent( cli_ctx );
	}
}

static void exit_kindly( struct cli_ctx *cli_ctx ) {
	cli_ctx_free( cli_ctx );
	puts( "Greeny is exiting normally." );
	exit( EXIT_SUCCESS );
}

static void cli_ctx_alloc( struct cli_ctx *cli_ctx ) {
	int in_err;

	memset( cli_ctx, 0, sizeof( struct cli_ctx ) );
	cli_ctx->files = vector_alloc( sizeof( char * ), &in_err );
	die_if( cli_ctx, in_err );
	cli_ctx->transforms = vector_alloc( sizeof( struct grn_transform ), &in_err );
	die_if( cli_ctx, in_err );
	cli_ctx->grn_ctx = grn_ctx_alloc( &in_err );
	die_if( cli_ctx, in_err );
}

static void cli_ctx_free_cats( struct cli_ctx *cli_ctx ) {
	grn_free( cli_ctx->files );
	grn_free( cli_ctx->transforms );
	cli_ctx->files = NULL;
	cli_ctx->transforms = NULL;
}

static void cli_ctx_free( struct cli_ctx *cli_ctx ) {
	int in_err;

	cli_ctx_free_cats( cli_ctx );
	grn_free( cli_ctx->orpheus_user_announce );
	if ( cli_ctx->grn_ctx != NULL ) {
		grn_ctx_free( cli_ctx->grn_ctx, &in_err );
		if ( in_err ) {
			printf( "Error freeing Greeny context: %s", grn_err_to_string( in_err ) );
		}
	}
}

static void handle_opts( struct cli_ctx *cli_ctx, int *argind, int argc, char **argv ) {
	char shortopts[] = "t:hv";
	struct option longopts[] = {
		{
			.name = "help",
			.has_arg = 0,
			.flag = NULL,
			.val = 'h',
		},
		{
			.name = "orpheus",
			.has_arg = 1,
			.flag = NULL,
			.val = 1337,
		},
#define X_CLIENT(x_machine, x_enum, x_human) { \
	.name = #x_machine, \
	.has_arg = 0, \
	.flag = &cli_ctx->x_machine, \
	.val = 1, \
},
#include "x_clients.h"
#undef X_CLIENT
		{ 0 },
	};

	int opt_c = 0;
	while ( ( opt_c = getopt_long( argc, argv, shortopts, longopts, NULL ) ) != -1 ) {
		switch ( opt_c ) {
			case 1337:
				;
				grn_free( cli_ctx->orpheus_user_announce );
				cli_ctx->orpheus_user_announce = malloc( strlen( optarg ) + 1 );
				if ( cli_ctx->orpheus_user_announce == NULL ) {
					die_if( cli_ctx, GRN_ERR_OOM );
				}
				strcpy( cli_ctx->orpheus_user_announce, optarg );
				break;
			// unknown option
			case '?':
				;
				die_if( cli_ctx, GRN_ERR_UNKNOWN_CLI_OPT );
				break;
			case 't':
				;
				puts( "Not implemented yet." );
				break;
			case 'h':
				;
				puts( help_text );
				exit_kindly( cli_ctx );
				break;
			case 'v':
				;
				puts( version_text );
				exit_kindly( cli_ctx );
				break;
				// other might mean 0, for a long opt. No need to handle it.
		}
	}

	*argind = optind;
}

static void cat_transforms( struct cli_ctx *cli_ctx ) {
	int in_err;

	// add client-specific files
#define X_CLIENT(x_machine, x_enum, x_human) if ( cli_ctx->x_machine ) { \
	grn_cat_client( cli_ctx->files, x_enum, &in_err); \
	die_if(cli_ctx, in_err); \
}
#include "x_clients.h"
#undef X_CLIENT

	if ( cli_ctx->orpheus_user_announce != NULL ) {
		grn_cat_transforms_orpheus( cli_ctx->transforms, cli_ctx->orpheus_user_announce, &in_err );
		die_if( cli_ctx, in_err );
	}
}

static void cat_files( struct cli_ctx *cli_ctx, int argind, int argc, char **argv ) {
	int in_err;

	// add normal files
	for ( ; argind < argc; argind++ ) {
		printf( "Adding %s and subdirectories.\n", argv[argind] );
		grn_cat_torrent_files( cli_ctx->files, argv[argind], NULL, &in_err );
		if ( grn_err_is_single_file( in_err ) ) {
			printf( "Error adding %s -- file may not exist.", argv[optind] );
			in_err = GRN_OK;
		}
		die_if( cli_ctx, in_err );
	}
}

static void seal( struct cli_ctx *cli_ctx ) {
	int files_n = vector_length( cli_ctx->files );
	int transforms_n = vector_length( cli_ctx->transforms );

	// TODO: should we have a defined error for this instead?
	if ( transforms_n == 0 ) {
		puts( "No transformations to apply. Try using --orpheus yourpasscode to convert from Apollo to Orpheus." );
		die_silent( cli_ctx );
	}

	printf( "About to process %d files with %d transformations.\n", files_n, transforms_n );
	if ( files_n == 0 ) {
		exit_kindly( cli_ctx );
	}

	grn_ctx_set_files_v( cli_ctx->grn_ctx, cli_ctx->files );
	grn_ctx_set_transforms_v( cli_ctx->grn_ctx, cli_ctx->transforms );
	cli_ctx->files = NULL;
	cli_ctx->transforms = NULL;

	cli_ctx_free_cats( cli_ctx );
}

static void main_loop( struct cli_ctx *cli_ctx ) {
	int in_err;
	int single_errors_count = 0;

	// on this blessed day, all files and transforms are in place. Let's do the thing!
	while ( true ) {
		char *next_file_path = grn_ctx_get_next_path( cli_ctx->grn_ctx );
		if ( next_file_path != NULL ) {
			printf( "Transforming file: %s\n", next_file_path );
		}
		if ( grn_one_file( cli_ctx->grn_ctx, &in_err ) ) {
			break;
		}
		int single_file_err = grn_ctx_get_c_error( cli_ctx->grn_ctx );
		if ( single_file_err ) {
			single_errors_count++;
			printf( "Single-file error: %s\n", grn_err_to_string( single_file_err ) );
		}
		die_if( cli_ctx, in_err );
	}

	if ( single_errors_count > 0 ) {
		printf( "Transformations complete with %d errors.\n", single_errors_count );
	} else {
		puts( "Transformations complete without error. Thank greeny." );
	}
}
