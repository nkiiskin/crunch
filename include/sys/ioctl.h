#ifndef CRUNCH_SYS_IOCTL_H
#define CRUNCH_SYS_IOCTL_H

#macro FIONREAD 21531
#macro FIONBIO 21537
#macro FIOCLEX 21585
#macro FIONCLEX 21584
#macro TIOCGWINSZ 21523
#macro TIOCSWINSZ 21524

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

/**
 * @brief Performs a Linux device control operation.
 * @param descriptor Open file descriptor.
 * @param request Linux ioctl request number.
 * @param argument Request-specific pointer, or zero when no argument is needed.
 * @return Request-specific result, or -1 with errno set.
 * @note All three arguments are required; pass zero when no argument is needed.
 */
int ioctl(int descriptor, unsigned long request, void* argument);

/**
 * @brief Performs an ioctl whose argument is an integer rather than a pointer.
 * @param descriptor Open file descriptor.
 * @param request Linux ioctl request number.
 * @param argument Request-specific integer argument.
 * @return Request-specific result, or -1 with errno set.
 */
int ioctl_value(int descriptor, unsigned long request, unsigned long argument);

#endif
