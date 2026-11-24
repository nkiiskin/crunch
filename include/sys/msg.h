#ifndef CRUNCH_SYS_MSG_H
#define CRUNCH_SYS_MSG_H

#include <sys/ipc.h>

#macro MSG_NOERROR 4096
#macro MSG_EXCEPT 8192
#macro MSG_COPY 16384
#macro MSG_STAT 11
#macro MSG_INFO 12
#macro MSG_STAT_ANY 13

typedef unsigned long msgqnum_t;
typedef unsigned long msglen_t;

/* Generic message prefix; actual message storage may have a larger text field. */
struct msgbuf {
    long mtype;
    char mtext[1];
};

struct msqid_ds {
    struct ipc_perm msg_perm;
    time_t msg_stime;
    time_t msg_rtime;
    time_t msg_ctime;
    unsigned long __msg_cbytes;
    msgqnum_t msg_qnum;
    msglen_t msg_qbytes;
    pid_t msg_lspid;
    pid_t msg_lrpid;
    unsigned long __reserved1;
    unsigned long __reserved2;
};

/**
 * @brief Gets or creates a System V message queue.
 * @param key IPC key or IPC_PRIVATE.
 * @param flags Permissions and IPC flags.
 * @return Queue identifier, or -1 with errno set.
 */
int msgget(key_t key, int flags);

/**
 * @brief Sends a System V message.
 * @param id Queue identifier.
 * @param message Message beginning with a positive long type.
 * @param size Size of message text, excluding its type.
 * @param flags Zero or IPC_NOWAIT.
 * @return Zero, or -1 with errno set.
 */
int msgsnd(int id, const void* message, size_t size, int flags);

/**
 * @brief Receives a System V message.
 * @param id Queue identifier.
 * @param message Destination beginning with a long type.
 * @param size Maximum message text size.
 * @param type Message type selector.
 * @param flags Linux message receive flags.
 * @return Number of text bytes received, or -1 with errno set.
 */
ssize_t msgrcv(int id, void* message, size_t size, long type, int flags);

/**
 * @brief Controls a System V message queue.
 * @param id Queue identifier.
 * @param command IPC_RMID, IPC_SET, or IPC_STAT.
 * @param buffer Queue information, or NULL.
 * @return Command result, or -1 with errno set.
 */
int msgctl(int id, int command, struct msqid_ds* buffer);

#endif
