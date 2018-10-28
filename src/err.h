#ifndef H_ERR
#define H_ERR

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

enum {
	GRN_OK = 0,
	GRN_ERR_OOM,
	GRN_ERR_FS_SEEK,
	GRN_ERR_FS_READ,
	GRN_ERR_FS_WRITE,
	GRN_ERR_FS_OPEN,
	GRN_ERR_FS_CLOSE,
	GRN_ERR_FS_NFTW,
	GRN_ERR_ENOENT,
	GRN_ERR_BENCODE_SYNTAX, // represents BEN errors 1, 2, 4. Ben error 0 is GRN_OK, 3 is GRN_ERR_OOM
	GRN_ERR_NO_CLIENT_PATH, // unable to determine the path for a client
	GRN_ERR_READ_CLIENT_PATH, // able to determine the path a client should have, but it did not exist
	GRN_ERR_REGEX_SYNTAX,
	GRN_ERR_ORPHEUS_ANNOUNCE_SYNTAX,
	GRN_ERR_UNKNOWN_CLI_OPT,
	GRN_ERR_USER_CANCELLED,
	GRN_ERR_NO_FILES,
};

static char *grn_err_to_string( int err ) {
	switch ( err ) {
#define X_ERR(en, hm) case en: return hm;
			X_ERR( GRN_OK, "Successful" )
			X_ERR( GRN_ERR_OOM, "Out of memory" )
			X_ERR( GRN_ERR_FS_SEEK, "Filesystem error: SEEK" )
			X_ERR( GRN_ERR_FS_READ, "Filesystem error: read" )
			X_ERR( GRN_ERR_FS_WRITE, "Filesystem error: write" )
			X_ERR( GRN_ERR_FS_OPEN, "Filesystem error: open" )
			X_ERR( GRN_ERR_FS_CLOSE, "Filesystem error: close" )
			X_ERR( GRN_ERR_FS_NFTW, "Filesystem error: nftw/recursive search" )
			X_ERR( GRN_ERR_ENOENT, "File/Directory not found" )
			X_ERR( GRN_ERR_BENCODE_SYNTAX, "Invalid bencode syntax" )
			X_ERR( GRN_ERR_NO_CLIENT_PATH, "Unable to determine torrent path for a client" )
			X_ERR( GRN_ERR_READ_CLIENT_PATH, "A torrent path for a client was unreadable" )
			X_ERR( GRN_ERR_REGEX_SYNTAX, "Invalid regex syntax" )
			X_ERR( GRN_ERR_ORPHEUS_ANNOUNCE_SYNTAX, "Invalid Orpheus announce URL/passkey syntax" )
			X_ERR( GRN_ERR_UNKNOWN_CLI_OPT, "Unrecognized CLI option" )
			X_ERR( GRN_ERR_USER_CANCELLED, "Operation cancelled" );
			X_ERR( GRN_ERR_NO_FILES, "No files or clients selected" );
#undef X_ERR
	};
	assert( false );
	return NULL;
}

/**
 * Determines if a context can continue processing after this error
 */
static bool grn_err_is_single_file( int err ) {
	return err == GRN_ERR_FS_SEEK ||
	       err == GRN_ERR_FS_READ ||
	       err == GRN_ERR_FS_WRITE ||
	       err == GRN_ERR_FS_OPEN ||
	       err == GRN_ERR_FS_CLOSE ||
	       err == GRN_ERR_ENOENT ||
	       err == GRN_ERR_BENCODE_SYNTAX;
}

#define ERR1(error)                do { \
                                      *out_err = error; \
                                      return; \
                                   } while (0)
#define ERR_NULL1(error)           do { \
                                      *out_err = error; \
                                      return 0; \
                                   } while (0)
#define ERR2(statement, error)      do { \
                                        if (statement) {       \
                                            *out_err = error;     \
                                            return;               \
                                        } \
                                    } while (0)
#define ERR_NULL2(statement, error)  do { \
                                        if (statement) {       \
                                            *out_err = error;     \
                                            return 0;               \
                                        } \
                                    } while (0)
#define GET_ERR_MACRO(_1, _2, MNAME, ...) MNAME
#define ERR(...) GET_ERR_MACRO(__VA_ARGS__, ERR2, ERR1)(__VA_ARGS__)
#define ERR_NULL(...) GET_ERR_MACRO(__VA_ARGS__, ERR_NULL2, ERR_NULL1)(__VA_ARGS__)
#define ERR_FW()                    do { \
                                        if (*out_err != GRN_OK) { \
                                            return; \
                                        } \
                                    } while (0)
#define ERR_FW_NULL()               do { \
                                        if (*out_err != GRN_OK) { \
                                            return 0; \
                                        } \
                                    } while (0)

#define RETURN_OK(retval) do { \
                              *out_err = GRN_OK; \
                              return retval; \
                          } while (0)

#define ERR_FW_CLEANUP() do { \
                            if (*out_err) { \
                                goto cleanup; \
                            } \
                         } while (0)

// GRN logging levels: 0 = none (default), 1 = error, 2 = warning, 3 = debug, 4 = debug plus
#ifndef GRN_LOG_FILE
#define GRN_LOG_FILE stderr
#endif

#define GRN_LOG(msg, level, ...) do { \
	if ( fprintf(GRN_LOG_FILE, "GREENY %s: '" msg "' at %s line %d\n", level, __VA_ARGS__, __FILE__, __LINE__) < 0 ) { \
		puts("Greeny failed to log -- make sure that GRN_LOG_FILE is writable."); \
	} \
} while (0)

#if GRN_LOG_LEVEL >= 4
#define GRN_LOG_DP(msg, ...) GRN_LOG(msg, "DEBUG PLUS", __VA_ARGS__)
#else
#define GRN_LOG_DP(...) ;
#endif

#if GRN_LOG_LEVEL >= 3
#define GRN_LOG_DEBUG(msg, ...) GRN_LOG(msg, "DEBUG", __VA_ARGS__)
#else
#define GRN_LOG_DEBUG(...) ;
#endif

#if GRN_LOG_LEVEL >= 2
#define GRN_LOG_WARNING(msg, ...) GRN_LOG(msg, "WARNING", __VA_ARGS__)
#else
#define GRN_LOG_WARNING(...) ;
#endif

#if GRN_LOG_LEVEL >= 1
#define GRN_LOG_ERROR(msg, ...) GRN_LOG(msg, "ERROR", __VA_ARGS__)
#else
#define GRN_LOG_ERROR(...) ;
#endif

#endif // H_ERR
