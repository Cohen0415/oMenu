#ifndef OMENU_LOG_H
#define OMENU_LOG_H

#define OMENU_LOG_INFO  0
#define OMENU_LOG_WARN  1
#define OMENU_LOG_ERROR 2
#define OMENU_LOG_DEBUG 3

#ifndef CONFIG_OMENU_LOG_LEVEL
#define CONFIG_OMENU_LOG_LEVEL OMENU_LOG_INFO
#endif

void omenu_log(int level, const char *fmt, ...);

#define OMENU_LOG(level, ...) \
    do { \
        if ((level) <= CONFIG_OMENU_LOG_LEVEL) \
            omenu_log((level), __VA_ARGS__); \
    } while (0)

#endif // OMENU_LOG_H
