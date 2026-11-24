#ifndef CRUNCH_SYS_SEM_H
#define CRUNCH_SYS_SEM_H

#include <sys/ipc.h>

#macro SEM_UNDO 4096
#macro GETPID 11
#macro GETVAL 12
#macro GETALL 13
#macro GETNCNT 14
#macro GETZCNT 15
#macro SETVAL 16
#macro SETALL 17
#macro SEM_STAT 18
#macro SEM_INFO 19
#macro SEM_STAT_ANY 20

struct sembuf {
    unsigned short sem_num;
    short sem_op;
    short sem_flg;
};

struct semid_ds {
    struct ipc_perm sem_perm;
    time_t sem_otime;
    unsigned long __sem_otime_high;
    time_t sem_ctime;
    unsigned long __sem_ctime_high;
    unsigned long sem_nsems;
    unsigned long __reserved1;
    unsigned long __reserved2;
};

/**
 * @brief Gets or creates a System V semaphore set.
 * @param key IPC key or IPC_PRIVATE.
 * @param count Number of semaphores in a new set.
 * @param flags Permissions and IPC flags.
 * @return Semaphore set identifier, or -1 with errno set.
 */
int semget(key_t key, int count, int flags);

/**
 * @brief Atomically performs semaphore operations.
 * @param id Semaphore set identifier.
 * @param operations Array of operations.
 * @param count Number of operations.
 * @return Zero, or -1 with errno set.
 */
int semop(int id, struct sembuf* operations, size_t count);

/**
 * @brief Controls a System V semaphore set using a pointer argument.
 * @param id Semaphore set identifier.
 * @param number Semaphore number.
 * @param command Linux semaphore control command.
 * @param argument Pointer to command-specific data, or NULL.
 * @return Command result, or -1 with errno set.
 */
int semctl(int id, int number, int command, void* argument);

/**
 * @brief Controls a semaphore using an integer command argument.
 * @param id Semaphore set identifier.
 * @param number Semaphore number.
 * @param command Linux semaphore control command.
 * @param argument Integer command argument.
 * @return Command result, or -1 with errno set.
 */
int semctl_value(int id, int number, int command, int argument);

#endif
