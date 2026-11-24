#ifndef CRUNCH_SYS_IPC_H
#define CRUNCH_SYS_IPC_H

#include <sys/types.h>

#macro IPC_PRIVATE 0
#macro IPC_CREAT 512
#macro IPC_EXCL 1024
#macro IPC_NOWAIT 2048
#macro IPC_RMID 0
#macro IPC_SET 1
#macro IPC_STAT 2
#macro IPC_INFO 3

struct ipc_perm {
    key_t __key;
    uid_t uid;
    gid_t gid;
    uid_t cuid;
    gid_t cgid;
    mode_t mode;
    unsigned short __seq;
    unsigned short __pad2;
    unsigned long __reserved1;
    unsigned long __reserved2;
};

#endif
