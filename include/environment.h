#ifndef CRUNCH_ENVIRONMENT_H
#define CRUNCH_ENVIRONMENT_H

/**
 * @brief Finds a value in a NULL-terminated vector of environment strings.
 * @param environment Environment entries in NAME=VALUE form, followed by NULL.
 * @param name NUL-terminated variable name to find.
 * @return Pointer to the value after '=', or NULL when the name is absent.
 */
char* env_get(char environment[][], const char* name);

#endif /* CRUNCH_ENVIRONMENT_H */
