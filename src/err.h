#ifndef H_ERR
#define H_ERR

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

#endif
