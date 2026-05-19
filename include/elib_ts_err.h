/* elib_ts_err.h - Task Scheduler Error Codes */
#ifndef ELIB_TS_ERR_H
#define ELIB_TS_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ELIB_TS_OK = 0,
    ELIB_TS_ERR_INVALID_PARAM,
    ELIB_TS_ERR_NOT_INITIALIZED,
    ELIB_TS_ERR_NOT_FOUND,
    ELIB_TS_ERR_FULL,
    ELIB_TS_ERR_NOT_RUNNING,
    ELIB_TS_ERR_ALREADY_RUNNING,
    ELIB_TS_ERR_TASK_STOPPED,
    ELIB_TS_ERR_TASK_RUNNING,
} elib_ts_err_t;

#ifdef __cplusplus
}
#endif

#endif /* ELIB_TS_ERR_H */
