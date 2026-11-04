#define _POSIX_C_SOURCE 200809L
#include "logger.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *log_file;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int logger_init(const char *path)
{
    int result = 0;

    if (path == NULL) {
        return -1;
    }

    (void)pthread_mutex_lock(&log_mutex);
    log_file = fopen(path, "a");
    if (log_file == NULL) {
        result = -1;
    }
    (void)pthread_mutex_unlock(&log_mutex);

    return result;
}

void logger_close(void)
{
    (void)pthread_mutex_lock(&log_mutex);
    if (log_file != NULL) {
        (void)fclose(log_file);
        log_file = NULL;
    }
    (void)pthread_mutex_unlock(&log_mutex);
}

void logger_write(const char *level, const char *format, ...)
{
    time_t now;
    struct tm time_info;
    char timestamp[32];

    if ((level == NULL) || (format == NULL)) {
        return;
    }

    now = time(NULL);
    if (localtime_r(&now, &time_info) == NULL) {
        return;
    }

    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
                 &time_info) == 0U) {
        return;
    }

    (void)pthread_mutex_lock(&log_mutex);
    if (log_file != NULL) {
        va_list arguments;
        (void)fprintf(log_file, "[%s] [%s] ", timestamp, level);
        va_start(arguments, format);
        (void)vfprintf(log_file, format, arguments);
        va_end(arguments);
        (void)fputc('\n', log_file);
        (void)fflush(log_file);
    }
    (void)pthread_mutex_unlock(&log_mutex);
}
