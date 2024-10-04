#pragma once

#include <cstdio>
#include "basic_types.h"

struct tm;
namespace nslib {
struct logging_ctxt;

dllapi extern logging_ctxt *GLOBAL_LOGGER;

struct log_event
{
    va_list ap;
    const char *fmt;
    const char *file;
    const char *func;
    tm *time;
    void *udata;
    int line;
    int level;
    u64 thread_id;
};

using logging_cbfn = void(log_event *ev);
using logging_lock_cbfn = void(bool lock, void *udata);

struct logging_cb_data
{
    logging_cbfn *fn;
    void *udata;
    int level;
};

struct lock_cb_data
{
    logging_lock_cbfn *fn;
    void *udata;
};

enum
{
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

#define tlog(...) lprint(GLOBAL_LOGGER, LOG_TRACE, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define dlog(...) lprint(GLOBAL_LOGGER, LOG_DEBUG, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define ilog(...) lprint(GLOBAL_LOGGER, LOG_INFO, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define wlog(...) lprint(GLOBAL_LOGGER, LOG_WARN, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define elog(...) lprint(GLOBAL_LOGGER, LOG_ERROR, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define flog(...) lprint(GLOBAL_LOGGER, LOG_FATAL, __FILE__, __func__, __LINE__, __VA_ARGS__)

logging_ctxt *create_logger(const char *name, int level, bool quiet);
void destroy_logger(logging_ctxt *logger);
const char *logging_level_string(logging_ctxt *logger, int level);
void set_logging_lock(logging_ctxt *logger, const lock_cb_data &cb_data);
void set_logging_level(logging_ctxt *logger, int level);
int logging_level(logging_ctxt *logger);
void set_quiet_logging(logging_ctxt *logger, bool enable);
int add_logging_fp(logging_ctxt *logger, FILE *fp, int level);
int add_logging_callback(logging_ctxt *logger, const logging_cb_data &cb_data);
void lprint(logging_ctxt *logger, int level, const char *file, const char *func, int line, const char *fmt, ...);

} // namespace nslib
