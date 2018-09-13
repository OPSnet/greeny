#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../contrib/dropt.h"
#include "libannouncebulk.h"
#include "vector.h"

char *version_text = "GREENY, Greeny Really Enjoys Editing torreNts, Yup!\n"
	"You are using the GREENY command line interface, version PRE_ALPHA\n"
	"This software's copyright is assigned to the public domain.\n";
char *help_text = "USAGE:\n"
	"\n"
	"greeny [ OPTIONS ] [ -- ] input_file_1 input_file\n"
	"If no options are specified, --orpheus is assumed. This behavior may be removed in a later version.\n"
	"\n"
	"OPTIONS:\n";

int main(int argc, char **argv) {
	grn_main_arg main_arg = { 0 };
	dropt_bool flag_help, flag_version;
	dropt_option cli_opts[] = {
		{ 'h', "help", "Shows help", NULL, dropt_handle_bool, &flag_help, dropt_attr_halt },
		{ 'v', "version", "Shows version and author information.", NULL, dropt_handle_bool, &flag_version, dropt_attr_halt },
		{ 0 },
	};
	dropt_context *dropt_ctx = dropt_new_context(cli_opts);
	if (dropt_ctx == NULL) {
		puts("Dropt initialization error");
		return 1;
	}
	if (argc < 1) {
		puts("You're running this in some really strange environment without a single CLI option!");
		return 1;
	}
	char **extra_args = dropt_parse(dropt_ctx, -1, &argv[1]);
	if (dropt_get_error(dropt_ctx) != dropt_error_none) {
		printf("CLI error: %s\n", dropt_get_error_message(dropt_ctx));
		return 1;
	}

	if (flag_help) {
		puts(help_text);
		dropt_print_help(stdout, dropt_ctx, NULL);
		return 0;
	}
	if (flag_version) {
		puts(version_text);
		return 0;
	}

	// all non-file arguments have been passed, gogogo!
	if (*extra_args == NULL) {
		puts("You must specify at least one file/folder to process.");
		return 1;
	}
	main_arg.paths = extra_args;

	// TODO: only proceed if no custom transforms specified.
	if (true) {
		
	}

	puts("no args passed.");
	return 0;
};
