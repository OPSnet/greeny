#ifndef H_ERR
#define H_ERR

#include <stdio.h>

enum {
	GRN_OK = 0,
	GRN_ERR_OOM,
	GRN_ERR_FS,
	GRN_ERR_WRONG_BENCODE_TYPE,
	GRN_ERR_WRONG_TRANSFORM_OPERATION,
	GRN_ERR_WRONG_CTX_STATE,
	GRN_ERR_WRONG_CLIENT,
	GRN_ERR_BENCODE_SYNTAX, // represents BEN errors 1, 2, 4. Ben error 0 is GRN_OK, 3 is GRN_ERR_OOM
	GRN_ERR_NO_CLIENT_PATH, // unable to determine the path for a client
	GRN_ERR_READ_CLIENT_PATH, // able to determine the path a client should have, but it did not exist
	GRN_ERR_REGEX_SYNTAX,
	GRN_ERR_ORPHEUS_ANNOUNCE_SYNTAX,
};


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
