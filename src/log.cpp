#include <stdio.h>
#include <time.h>
#include "log.h"

static uint32_t s_LogLevel = LOG_INFO;

static void logCbDefault(uint32_t prio, const char* tag, const char *fmt, va_list ap)
{
    char buf[128];
    struct timespec ts;
    FILE *stream;
    char strPrio;

    if (prio > s_LogLevel) {
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);

    switch (prio) {
    case LOG_CRIT:
        strPrio = 'C';
        stream = stderr;
        break;

    case LOG_ERR:
        strPrio = 'E';
        stream = stderr;
        break;

    case LOG_WARN:
        strPrio = 'W';
        stream = stderr;
        break;

    case LOG_NOTICE:
        strPrio = 'N';
        stream = stdout;
        break;

    case LOG_INFO:
        strPrio = 'I';
        stream = stdout;
        break;

    case LOG_DEBUG:
        strPrio = 'D';
        stream = stdout;
        break;

    default:
        strPrio = 'C';
        stream = stderr;
        break;
    }

    vsnprintf(buf, sizeof(buf), fmt, ap);
    fprintf(stream, "[%lu:%03lu][%c][%-8s] %s\n", ts.tv_sec, ts.tv_nsec / 1000000, strPrio, tag, buf);
}

static log_cb_t sCb = logCbDefault;

void __log(uint32_t prio, const char* tag, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    sCb(prio, tag, fmt, ap);
    va_end(ap);
}

void logSetLevel(uint32_t prio)
{
    s_LogLevel = prio;
}
