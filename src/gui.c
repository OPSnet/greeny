#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <iup.h>

#include "libannouncebulk.h"
#include "vector.h"
#include "util.h"
#include "err.h"
#include "about.h"

Ihandle *main_dlg = NULL,
         *file_list,
         *orpheus_field,
         *progress_dlg = NULL
                         ;

struct vector *ui_files = NULL;
struct grn_ctx *grn_run_ctx = NULL;

static void ui_open();

static void exit_with_code( int code );
static void exit_kindly();
static void exit_badly();
static void exit_if_err( int err );
static void popup_err( int err );

static void show_text_dlg( const char *title, const char *content );
static void setup_main_dlg();
static void setup_progress_dlg();
static void setup_dlgs();

static void summarize();
static void progress_loop();
static void add_file( const char *path );
static void cat_files_to_runner();
static void cat_transforms_to_runner( int *out_err );
static void seal();

static int cb_quit();
static int cb_about();
static int cb_run();
static int cb_drop_file( Ihandle *ih, const char *path );
#define X_CLIENT(var, enum, human) Ihandle *var##_checkbox; \
	bool var##_val; \
	static int cb_##var(Ihandle *ih, int toggle_status) { \
		var##_val = (bool) toggle_status; \
		return IUP_DEFAULT; \
	}
#include "x_clients.h"
#undef X_CLIENT

int main( int argc, char **argv ) {
	IupOpen( &argc, &argv );

	ui_open();
	setup_dlgs();

	IupMainLoop();

	exit_kindly();
}

static void ui_open() {
	int in_err;

	ui_files = vector_alloc( sizeof( char * ), &in_err );
	exit_if_err( in_err );
}

static void exit_with_code( int code ) {
	int in_err;

	vector_free( ui_files );
	grn_ctx_free( grn_run_ctx, &in_err );
	if ( main_dlg != NULL ) {
		IupDestroy( main_dlg );
	}
	if ( progress_dlg != NULL ) {
		IupDestroy( progress_dlg );
	}
	IupClose();
	puts( "Exiting properly." );
	exit( code );
}

static void exit_kindly() {
	exit_with_code( EXIT_SUCCESS );
}

static void exit_badly() {
	exit_with_code( EXIT_FAILURE );
}

static void exit_if_err( int err ) {
	if ( err ) {
		printf( "GREENY errror: %s.", grn_err_to_string( err ) );
		exit_badly();
	}
}

static void popup_err( int err ) {
	show_text_dlg( "Error", grn_err_to_string( err ) );
}

static int cb_quit() {
	return IUP_CLOSE;
}

static int cb_about() {
	Ihandle *about_dlg = IupMessageDlg();
	IupSetAttribute( about_dlg, "VALUE", GRN_ABOUT_TEXT );
	IupSetAttribute( about_dlg, "PARENTDIALOG", "main_dlg" );
	IupPopup( about_dlg, IUP_CENTERPARENT, IUP_CENTERPARENT );
	return IUP_DEFAULT;
}

static int cb_run() {
	int in_err;

	seal( &in_err );
	if ( in_err ) {
		popup_err( in_err );
		return IUP_DEFAULT;
	}
	IupShowXY( progress_dlg, IUP_CENTERPARENT, IUP_CENTERPARENT );
	progress_loop( &in_err );
	if ( in_err ) {
		// TODO: the IupLoopStep causes the main loop to exit if IUP_CLOSE is returned by anything (the cancel button), which isn't
		// what the docs say and isn't desirable, either.
		popup_err( in_err );
		return IUP_DEFAULT;
	}
	IupHide( progress_dlg );
	summarize();
	return IUP_DEFAULT;
}

// for whatever reason, IUP uses url-encoding on filenames, which makes
// special characters, well, special.
static int cb_drop_file( Ihandle *ih, const char *path ) {
	int in_err;

	char *utf8_path = grn_malloc( strlen( path ) + 1, &in_err );
	exit_if_err( in_err );
	grn_decode_url( utf8_path, path );
	add_file( utf8_path );
	// yeah, it won't get freed right if add_file fails, but who cares, not GTK for one
	free( utf8_path );
	return IUP_DEFAULT;
}

static void add_file( const char *path ) {
	int in_err;

	char *copied_path = grn_strcpy_malloc( path, &in_err );
	// these things can only fail with OOM, so we're safe!
	exit_if_err( in_err );
	vector_push( ui_files, &copied_path, &in_err );
	exit_if_err( in_err );

	Ihandle *label = IupLabel( path );
	IupSetAttribute( label, "PADDING", "10x0" );
	IupAppend( file_list, label );
	IupMap( label );
	IupRefresh( label );
}

static void cat_files_to_runner( int *out_err ) {
	*out_err = GRN_OK;

	struct vector *tmp_all_files = vector_alloc( sizeof( char * ), out_err );
	ERR_FW();
	for ( int i = 0; i < vector_length( ui_files ); i++ ) {
		char *this_ui_file = * ( char ** ) vector_get( ui_files, i );
		GRN_LOG_DEBUG( "Sealing with UI file: '%s'", this_ui_file );
		grn_cat_torrent_files( tmp_all_files, this_ui_file, NULL, out_err );
		if ( *out_err ) {
			if ( grn_err_is_single_file( *out_err ) ) {
				popup_err( *out_err );
				*out_err = GRN_OK;
			} else {
				return;
			}
		}
	}

#define X_CLIENT(var, enum, human) if (var##_val) { \
	grn_cat_client( tmp_all_files, enum, out_err ); \
	ERR_FW_CLEANUP(); \
}
#include "x_clients.h"
#undef X_CLIENT

	grn_ctx_set_files_v( grn_run_ctx, tmp_all_files );
	return;
cleanup:
	vector_free_all( tmp_all_files );
}

static void cat_transforms_to_runner( int *out_err ) {
	// we have a separate in_err because some errors cause a program exit, while some are forwarded
	*out_err = GRN_OK;
	int in_err;

	char *orpheus_user_announce = IupGetAttribute( orpheus_field, "VALUE" );
	struct vector *tmp_all_transforms = vector_alloc( sizeof( struct grn_transform ), &in_err );
	exit_if_err( in_err );

	grn_cat_transforms_orpheus( tmp_all_transforms, orpheus_user_announce, &in_err );
	if ( in_err == GRN_ERR_ORPHEUS_ANNOUNCE_SYNTAX ) {
		*out_err = in_err;
		vector_free( tmp_all_transforms );
		return;
	}
	if ( in_err ) {
		vector_free( tmp_all_transforms );
		exit_badly();
	}

	grn_ctx_set_transforms_v( grn_run_ctx, tmp_all_transforms );
}

static void seal( int *out_err ) {
	*out_err = GRN_OK;

	grn_ctx_free( grn_run_ctx, out_err );
	ERR_FW();
	grn_run_ctx = grn_ctx_alloc( out_err );
	ERR_FW();
	cat_transforms_to_runner( out_err );
	ERR_FW();
	cat_files_to_runner( out_err );
	ERR_FW();
}

static void progress_loop( int *out_err ) {
	*out_err = GRN_OK;
	int in_err;
	assert( grn_run_ctx != NULL );

	IupSetInt( progress_dlg, "TOTALCOUNT", grn_ctx_get_files_n( grn_run_ctx ) );
	while ( ! grn_ctx_get_is_done( grn_run_ctx ) ) {
		grn_one_file( grn_run_ctx, &in_err );
		exit_if_err( in_err );
		IupSetInt( progress_dlg, "COUNT", grn_ctx_get_files_c( grn_run_ctx ) );
		if ( IupLoopStep() == IUP_CLOSE ) {
			*out_err = GRN_ERR_USER_CANCELLED;
			return;
		}
	}
}

static void summarize() {
	assert( grn_run_ctx != NULL );

	char summary_text[512];
	sprintf( summary_text, "Done.\n%d files transformed, %d had errors.", grn_ctx_get_files_n( grn_run_ctx ), grn_ctx_get_errs_n( grn_run_ctx ) );

	show_text_dlg( "Transforms complete", summary_text );
}

static void show_text_dlg( const char *title, const char *content ) {
	Ihandle *msg_dlg = IupMessageDlg();
	IupSetAttribute( msg_dlg, "PARENTDIALOG", "main_dlg" );
	IupSetAttribute( msg_dlg, "TITLE", title );
	IupSetAttribute( msg_dlg, "VALUE", content );
	IupPopup( msg_dlg, IUP_CENTERPARENT, IUP_CENTERPARENT );
	IupDestroy( msg_dlg );
}

static void setup_progress_dlg() {
	progress_dlg = IupProgressDlg();
	IupSetAttribute( progress_dlg, "PARENTDIALOG", "main_dlg" );
	IupSetAttribute( progress_dlg, "TITLE", "Greeny at work" );
	IupSetAttribute( progress_dlg, "DESCRIPTION", "Transforming your torrents" );
	IupSetCallback( progress_dlg, "CANCEL_CB", ( Icallback )cb_quit );
}

static void setup_main_dlg() {
	////// RUN BUTTONS //////
	Ihandle *quit_button = IupButton( "Quit", NULL );
	IupSetAttribute( quit_button, "EXPAND", "HORIZONTAL" );
	IupSetCallback( quit_button, "ACTION", ( Icallback )cb_quit );

	Ihandle *about_button = IupButton( "About", NULL );
	IupSetAttribute( about_button, "EXPAND", "HORIZONTAL" );
	IupSetCallback( about_button, "ACTION", ( Icallback )cb_about );

	Ihandle *run_button = IupButton( "Run", NULL );
	IupSetAttribute( run_button, "EXPAND", "HORIZONTAL" );
	IupSetCallback( run_button, "ACTION", ( Icallback )cb_run );

	Ihandle *run_buttons_hbox = IupHbox( quit_button, about_button, run_button, 0 );
	IupSetAttribute( run_buttons_hbox, "ALIGNMENT", "ABOTTOM" );
	IupSetAttribute( run_buttons_hbox, "GAP", "10" );

	////// FILE LIST //////
	Ihandle *wide_file_item = IupLabel( "" );
	IupSetAttribute( wide_file_item, "EXPAND", "HORIZONTAL" );
	file_list = IupVbox( wide_file_item );
	Ihandle *file_list_frame = IupFrame( file_list );
	IupSetAttribute( file_list_frame, "TITLE", "To transform" );

	////// FILE BUTTONS //////
	/*
	add_file_button = IupButton( "Add File", NULL );
	IupSetAttribute( add_file_button, "EXPAND", "HORIZONTAL" );
	IupSetCallback( add_file_button, "ACTION", &cb_select_file );
	*/

	////// CLIENT CHECKBOXES //////
#define X_CLIENT(var, enum, human) var##_checkbox = IupToggle(human, NULL); \
	IupSetCallback(var##_checkbox, "ACTION", ( Icallback )cb_##var);
#include "x_clients.h"
#undef X_CLIENT

	////// ORPHEUS TEXTBOX ///////
	orpheus_field = IupText( NULL );
	IupSetAttribute( orpheus_field, "CUEBANNER", "abcdef0123456789abcdef0123456789" );
	IupSetAttribute( orpheus_field, "EXPAND", "HORIZONTAL" );

	Ihandle *main_vbox = IupVbox(
	                         IupLabel( "Drag files into Greeny (optional):" ),
	                         file_list_frame,
//	                add_file_button,

	                         IupLabel( "Automatically transform all torrents for popular clients (optional):" ),
#define X_CLIENT(var, enum, human) var##_checkbox,
#include "x_clients.h"
#undef X_CLIENT
	                         IupLabel( "Orpheus passphrase or announce URL:" ),
	                         orpheus_field,
	                         run_buttons_hbox,
	                         0
	                     );
	IupSetAttribute( main_vbox, "MARGIN", "10x10" );
	IupSetAttribute( main_vbox, "GAP", "5" );

	main_dlg = IupDialog( main_vbox );
	IupSetHandle( "main_dlg", main_dlg );
	IupSetAttribute( main_dlg, "MINSIZE", "300x450" );
	IupSetAttribute( main_dlg, "SHRINK", "YES" );
	IupSetAttribute( main_dlg, "TITLE", "GREENY" );
	IupSetCallback( main_dlg, "DROPFILES_CB", ( Icallback ) cb_drop_file );
	IupShow( main_dlg );
}

static void setup_dlgs() {
	setup_main_dlg();
	setup_progress_dlg();
}


