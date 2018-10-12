#include <stdlib.h>
#include <iup.h>

Ihandle *main_dlg = NULL,
         *main_vbox,
         *add_file_button, *add_dir_button,
         *run_button, *quit_button,
         *file_buttons_hbox, *run_buttons_hbox,
         *file_list,
         *orpheus_label, *orpheus_field,
         *progress_dlg = NULL,
          *file_dlg = NULL,
           *dir_dlg = NULL
                      ;

static void exit_with_code( int code );
static void exit_kindly();

static void setup_main_dlg();
static void setup_progress_dlg();
static void setup_file_dlgs();

static int cb_quit();
static int cb_run();
static int cb_select_file();

int main( int argc, char **argv ) {
	IupOpen( &argc, &argv );

	setup_progress_dlg();
	setup_file_dlgs();
	setup_main_dlg();

	IupMainLoop();

	exit_kindly();
}

static void exit_with_code( int code ) {
	if ( main_dlg != NULL ) {
		IupDestroy( main_dlg );
	}
	if ( progress_dlg != NULL ) {
		IupDestroy( progress_dlg );
	}
	IupClose();
	exit( code );
}

static void exit_kindly() {
	exit_with_code( EXIT_SUCCESS );
}

static int cb_quit() {
	return IUP_CLOSE;
}

static int cb_run() {
	IupPopup( progress_dlg, IUP_MOUSEPOS, IUP_MOUSEPOS );
	return IUP_DEFAULT;
}

static int cb_select_file() {
	IupPopup( file_dlg, IUP_MOUSEPOS, IUP_MOUSEPOS );
	return IUP_DEFAULT;
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

	////// FILE BUTTONS //////
	add_file_button = IupButton( "Add File", NULL );
	IupSetAttribute( add_file_button, "EXPAND", "HORIZONTAL" );
	IupSetCallback( add_file_button, "ACTION", ( Icallback )cb_select_file );

	main_vbox = IupVbox(
	                add_file_button,
	                run_buttons_hbox,
	                0
	            );
	IupSetAttribute( main_vbox, "MARGIN", "10x10" );
	IupSetAttribute( main_vbox, "GAP", "5" );

	main_dlg = IupDialog( main_vbox );
	IupSetAttribute( main_dlg, "TITLE", "GREENY" );
	IupShow( main_dlg );
}

