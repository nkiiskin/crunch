#ifndef CRUNCH_SYS_SYSINFO_H
#define CRUNCH_SYS_SYSINFO_H

#macro SI_LOAD_SHIFT 16

struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
};

/**
 * @brief Reads Linux system uptime, load, memory and process statistics.
 * @param information Receives the Linux x86-64 sysinfo structure.
 * @return Zero, or -1 with errno set.
 * @note Memory sizes are expressed in units of mem_unit bytes.
 */
int sysinfo(struct sysinfo* information);

#endif
