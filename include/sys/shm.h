#ifndef CRUNCH_SYS_SHM_H
#define CRUNCH_SYS_SHM_H

#include <sys/ipc.h>
#include <sys/mman.h>

#macro SHM_R 256
#macro SHM_W 128
#macro SHM_RDONLY 4096
#macro SHM_RND 8192
#macro SHM_REMAP 16384
#macro SHM_EXEC 32768
#macro SHM_LOCK 11
#macro SHM_UNLOCK 12
#macro SHM_HUGETLB 2048
#macro SHM_NORESERVE 4096

typedef unsigned long shmatt_t;

struct shmid_ds {
    struct ipc_perm shm_perm;
    size_t shm_segsz;
    time_t shm_atime;
    time_t shm_dtime;
    time_t shm_ctime;
    pid_t shm_cpid;
    pid_t shm_lpid;
    shmatt_t shm_nattch;
    unsigned long __reserved1;
    unsigned long __reserved2;
};

/**
 * @brief Gets or creates a System V shared memory segment.
 * @param key Segment key or IPC_PRIVATE.
 * @param size Requested segment size.
 * @param flags Permissions and IPC_* or SHM_* flags.
 * @return Segment identifier, or -1 with errno set.
 */
int shmget(key_t key, size_t size, int flags);

/**
 * @brief Attaches a System V shared memory segment.
 * @param id Segment identifier.
 * @param address Requested address, or NULL.
 * @param flags Linux SHM_* attachment flags.
 * @return Attachment address, or the -1 pointer sentinel with errno set.
 * @note Use map_failed to check the returned sentinel.
 */
void* shmat(int id, const void* address, int flags);

/**
 * @brief Controls a System V shared memory segment.
 * @param id Segment identifier.
 * @param command IPC_RMID, IPC_SET, IPC_STAT, SHM_LOCK, or SHM_UNLOCK.
 * @param buffer Segment information for IPC_SET or IPC_STAT, otherwise NULL.
 * @return Zero, or -1 with errno set.
 */
int shmctl(int id, int command, struct shmid_ds* buffer);

/**
 * @brief Detaches a System V shared memory segment.
 * @param address Address returned by shmat.
 * @return Zero, or -1 with errno set.
 */
int shmdt(const void* address);

#endif
