#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

enum LogLevel : uint32_t {
    LOG_CRIT,
    LOG_ERR,
    LOG_WARN,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG
};

#define LOGC(_tag, ...) LOG_PRI(LOG_CRIT,   _tag, __VA_ARGS__)
#define LOGE(_tag, ...) LOG_PRI(LOG_ERR,    _tag, __VA_ARGS__)
#define LOGW(_tag, ...) LOG_PRI(LOG_WARN,   _tag, __VA_ARGS__)
#define LOGN(_tag, ...) LOG_PRI(LOG_NOTICE, _tag, __VA_ARGS__)
#define LOGI(_tag, ...) LOG_PRI(LOG_INFO,   _tag, __VA_ARGS__)
#define LOGD(_tag, ...) LOG_PRI(LOG_DEBUG,  _tag, __VA_ARGS__)

#define LOG_PRI(_prio, ...)  __log(_prio, __VA_ARGS__)

#define LOG_ERRNO(_tag, _func) \
    LOGE(_tag, "%s:%d - %s(): err=%d(%s)", __func__, __LINE__, \
            _func, errno, strerror(errno))

#ifdef __MINGW32__
#define PRITime "lld"
#else
#define PRITime "lu"
#endif

typedef void (*log_cb_t) (uint32_t prio, const char* tag, const char *fmt, va_list ap);

void __log(uint32_t prio, const char* tag, const char *fmt, ...) __attribute__ ((format (gnu_printf, 3, 4)));

void logSetLevel(uint32_t prio);
