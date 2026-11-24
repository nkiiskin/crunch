#ifndef CRUNCH_SYS_PERSONALITY_H
#define CRUNCH_SYS_PERSONALITY_H

/**
 * @brief Sets the execution-domain personality; pass 0xffffffffUL to query it without changing it.
 * @param persona Linux personality, or 0xffffffffUL to query.
 * @return The result, or -1 with errno set.
 */
int personality(unsigned long persona);
#endif
