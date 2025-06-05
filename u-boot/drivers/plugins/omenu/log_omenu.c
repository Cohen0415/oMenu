#include "log_omenu.h"
#include <stdio.h>
#include <stdarg.h>

static const char *omenu_log_level_str(int level) 
{
    switch (level) 
    {
        case OMENU_LOG_INFO:  return "INFO";
        case OMENU_LOG_WARN:  return "WARN";
        case OMENU_LOG_ERROR: return "ERROR";
        case OMENU_LOG_DEBUG: return "DEBUG";
        default:              return "UNKNOWN";
    }
}

void omenu_log(int level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    const char *level_str = omenu_log_level_str(level);
    printf("oMenu [%s] : ", level_str);
    vprintf(fmt, args);

    va_end(args);
}
