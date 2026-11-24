#ifndef CRUNCH_SYS_FUTEX_H
#define CRUNCH_SYS_FUTEX_H
#include <time.h>

#macro FUTEX_WAIT 0
#macro FUTEX_WAKE 1
#macro FUTEX_REQUEUE 3
#macro FUTEX_CMP_REQUEUE 4
#macro FUTEX_WAKE_OP 5
#macro FUTEX_LOCK_PI 6
#macro FUTEX_UNLOCK_PI 7
#macro FUTEX_TRYLOCK_PI 8
#macro FUTEX_WAIT_BITSET 9
#macro FUTEX_WAKE_BITSET 10
#macro FUTEX_WAIT_REQUEUE_PI 11
#macro FUTEX_CMP_REQUEUE_PI 12
#macro FUTEX_LOCK_PI2 13
#macro FUTEX_PRIVATE_FLAG 128
#macro FUTEX_CLOCK_REALTIME 256
#macro FUTEX_BITSET_MATCH_ANY 0xffffffffU

/**
 * @brief Invokes Linux futex with a timeout pointer as the fourth argument.
 * @param address Futex word or mapping address.
 * @param operation Linux operation selector.
 * @param value Operation-specific value or futex comparison value.
 * @param timeout Optional timeout.
 * @param address2 Second futex word.
 * @param value3 Operation-specific sixth argument.
 * @return The syscall result, or -1 with errno set.
 */
int futex(int* address, int operation, unsigned int value, const struct timespec* timeout, int* address2, unsigned int value3);

/**
 * @brief Invokes Linux futex with an integer fourth argument, for operations such as FUTEX_REQUEUE and FUTEX_WAKE_OP.
 * @param address Futex word or mapping address.
 * @param operation Linux operation selector.
 * @param value Operation-specific value or futex comparison value.
 * @param value2 Operation-specific integer fourth argument.
 * @param address2 Second futex word.
 * @param value3 Operation-specific sixth argument.
 * @return The syscall result, or -1 with errno set.
 */
int futex_value(int* address, int operation, unsigned int value, unsigned long value2, int* address2, unsigned int value3);

#endif
