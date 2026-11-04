#ifndef LOGGER_H
#define LOGGER_H

int logger_init(const char *path);
void logger_close(void);
void logger_write(const char *level, const char *format, ...);

#endif
