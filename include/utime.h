#ifndef CRUNCH_UTIME_H
#define CRUNCH_UTIME_H

#include <sys/types.h>
struct utimbuf {
    time_t actime;
    time_t modtime;
};
/**
 * @brief Changes file access and modification times in whole seconds; NULL selects the current time.
 * @param path NUL-terminated path.
 * @param times New times, or NULL.
 * @return The result, or -1 with errno set.
 */
int utime(const char* path, const struct utimbuf* times);
#endif
