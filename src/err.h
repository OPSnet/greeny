#ifndef H_ERR
#define H_ERR

enum {
	GRN_OK = 0,
	GRN_ERR_OOM,
	GRN_ERR_FS,
	GRN_ERR_WRONG_BENCODE_TYPE,
	GRN_ERR_WRONG_TRANSFORM_OPERATION,
	// represents BEN errors 1, 2, 4. Ben error 0 is GRN_OK, 3 is GRN_ERR_OOM
	GRN_ERR_BENCODE_SYNTAX,
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
