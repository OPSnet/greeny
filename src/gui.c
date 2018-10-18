#include <stdlib.h>
#include <iup.h>

#include "libannouncebulk.h"
#include "vector.h"
#include "util.h"
#include "err.h"

Ihandle *main_dlg = NULL,
         *main_vbox,
         *add_file_button, *add_dir_button,
         *run_button, *quit_button,
         *file_buttons_hbox, *run_buttons_hbox,
         *file_list_frame, *file_list,
         *orpheus_field,
         *progress_dlg = NULL,
          *file_dlg = NULL,
           *dir_dlg = NULL
                      ;

struct vector *ui_files = NULL;
struct grn_ctx *grn_run_ctx = NULL;

static void ui_open();

static void exit_with_code( int code );
static void exit_kindly();
static void exit_badly();
static void exit_if_err( int err );
static void popup_err( int err );

static void setup_main_dlg();
static void setup_progress_dlg();
static void setup_file_dlgs();
static void setup_dlgs();

static void add_file( const char *path );
static void seal();

static int cb_quit();
static int cb_run();
static int cb_select_file();
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
	puts("Exiting properly.");
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
		exit_badly();
	}
}

static void popup_err( int err ) {
	Ihandle *popup_dlg = IupMessageDlg();
	IupSetAttribute( popup_dlg, "TITLE", "Error" );
	IupSetAttribute( popup_dlg, "VALUE", grn_err_to_string( err ) );
	IupPopup( popup_dlg, IUP_CENTER, IUP_CENTER );
}

static int cb_quit() {
	return IUP_CLOSE;
}

static int cb_run() {
	seal();
	IupPopup( progress_dlg, IUP_MOUSEPOS, IUP_MOUSEPOS );
	return IUP_DEFAULT;
}

static int cb_select_file() {
	IupPopup( file_dlg, IUP_MOUSEPOS, IUP_MOUSEPOS );
	return IUP_DEFAULT;
}

static int cb_drop_file( Ihandle *ih, const char *path ) {
	add_file( path );
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

static void cat_files_to_runner() {
	int in_err;

	struct vector *tmp_all_files = vector_alloc( sizeof( char * ), &in_err );
	exit_if_err( in_err );
	for ( int i = 0; i < vector_length( ui_files ); i++ ) {
		// TODO: fastresume and resume.dat
		grn_cat_torrent_files( tmp_all_files, vector_get( ui_files, i ), NULL, &in_err );
		if ( in_err ) {
			goto cleanup_err;
		}
	}

#define X_CLIENT(var, enum, human) if (var##_val) { \
	grn_cat_client(tmp_all_files, enum, &in_err); \
	if (in_err) { \
		goto cleanup_err; \
	} \
}
#include "x_clients.h"
#undef X_CLIENT

	grn_ctx_set_files_v( grn_run_ctx, tmp_all_files );
	return;
cleanup_err:
	vector_free_all( tmp_all_files );
	exit_badly();
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

static void seal() {
	int in_err;

	grn_ctx_free( grn_run_ctx, &in_err );
	exit_if_err( in_err );
	grn_run_ctx = grn_ctx_alloc( &in_err );
	exit_if_err( in_err );
	cat_files_to_runner();
	cat_transforms_to_runner( &in_err );
	if ( in_err ) {
		popup_err( in_err );
		// ctx will be freed on app exit if not sooner
		return;
	}
}

static void setup_file_dlgs() {
	file_dlg = IupFileDlg();
	IupSetAttribute( file_dlg, "DIALOGTYPE", "OPEN" );
	IupSetAttribute( file_dlg, "MULTIPLEFILES", "YES" );
	IupSetAttribute( file_dlg, "FILTER", "*.torrent;*.fastresume;*.resume;resume.dat" );
	IupSetAttribute( file_dlg, "FILTERINFO", "Torrent and fastresume files" );
}

static void setup_progress_dlg() {
	progress_dlg = IupProgressDlg();
	IupSetAttribute( progress_dlg, "TITLE", "Greeny at work" );
	IupSetAttribute( progress_dlg, "DESCRIPTION", "Transforming your torrents" );
	IupSetCallback( progress_dlg, "CANCEL_CB", ( Icallback )cb_quit );
}

static void setup_main_dlg() {
	////// RUN BUTTONS //////
	quit_button = IupButton( "Quit", NULL );
	IupSetAttribute( quit_button, "EXPAND", "HORIZONTAL" );
	IupSetCallback( quit_button, "ACTION", ( Icallback )cb_quit );

	run_button = IupButton( "Run", NULL );
	IupSetAttribute( run_button, "EXPAND", "HORIZONTAL" );
	IupSetCallback( run_button, "ACTION", ( Icallback )cb_run );

	run_buttons_hbox = IupHbox( quit_button, run_button, 0 );
	IupSetAttribute( run_buttons_hbox, "ALIGNMENT", "ABOTTOM" );
	IupSetAttribute( run_buttons_hbox, "GAP", "10" );

	////// FILE LIST //////
	file_list = IupVbox( NULL );
	IupSetAttribute( file_list, "GAP", "5" );
	file_list_frame = IupFrame( file_list );
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

	main_vbox = IupVbox(
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
	IupSetAttribute( main_vbox, "MARGIN", "0x10" );
	IupSetAttribute( main_vbox, "GAP", "5" );

	main_dlg = IupDialog( main_vbox );
	IupSetAttribute( main_dlg, "MINSIZE", "300x350" );
	IupSetAttribute( main_dlg, "SHRINK", "YES" );
	IupSetAttribute( main_dlg, "TITLE", "GREENY" );
	IupSetCallback( main_dlg, "DROPFILES_CB", ( Icallback ) cb_drop_file );
	IupShow( main_dlg );
}

static void setup_dlgs() {
	setup_file_dlgs();
	setup_progress_dlg();
	setup_main_dlg();
}


