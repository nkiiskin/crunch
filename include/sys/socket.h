#ifndef CRUNCH_SYS_SOCKET_H
#define CRUNCH_SYS_SOCKET_H
#include <stddef.h>
#include <sys/types.h>
#include <sys/uio.h>

typedef unsigned int socklen_t;
typedef unsigned short sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};
struct msghdr {
    void* msg_name;
    socklen_t msg_namelen;
    struct iovec* msg_iov;
    size_t msg_iovlen;
    void* msg_control;
    size_t msg_controllen;
    int msg_flags;
};

#macro AF_UNIX 1
#macro AF_INET 2
#macro AF_INET6 10
#macro SOCK_STREAM 1
#macro SOCK_DGRAM 2
#macro SOCK_NONBLOCK 2048
#macro SOCK_CLOEXEC 524288
#macro MSG_PEEK 2
#macro MSG_DONTWAIT 64
#macro MSG_NOSIGNAL 16384
#macro MSG_TRUNC 32
#macro SHUT_RD 0
#macro SHUT_WR 1
#macro SHUT_RDWR 2
#macro SOL_SOCKET 1
#macro SO_DEBUG 1
#macro SO_REUSEADDR 2
#macro SO_TYPE 3
#macro SO_ERROR 4
#macro SO_DONTROUTE 5
#macro SO_BROADCAST 6
#macro SO_SNDBUF 7
#macro SO_RCVBUF 8
#macro SO_KEEPALIVE 9
#macro SO_OOBINLINE 10
#macro SO_LINGER 13
#macro SO_REUSEPORT 15
#macro SO_RCVLOWAT 18
#macro SO_SNDLOWAT 19
#macro SO_RCVTIMEO 20
#macro SO_SNDTIMEO 21
#macro SO_ACCEPTCONN 30

struct linger {
    int l_onoff;
    int l_linger;
};

/**
 * @brief Creates a Linux socket.
 * @param domain Address family.
 * @param type Socket type and optional flags.
 * @param protocol Protocol number, usually zero.
 * @return Descriptor, or -1 with errno set.
 */
int socket(int domain, int type, int protocol);
/**
 * @brief Binds a socket to an address.
 * @param descriptor Socket descriptor.
 * @param address Address bytes.
 * @param length Address length in bytes.
 * @return Zero, or -1 with errno set.
 */
int bind(int descriptor, const struct sockaddr* address, socklen_t length);
/**
 * @brief Connects a socket to an address.
 * @param descriptor Socket descriptor.
 * @param address Address bytes.
 * @param length Address length in bytes.
 * @return Zero, or -1 with errno set.
 */
int connect(int descriptor, const struct sockaddr* address, socklen_t length);
/**
 * @brief Marks a socket as listening.
 * @param descriptor Socket descriptor.
 * @param backlog Maximum pending connection backlog.
 * @return Zero, or -1 with errno set.
 */
int listen(int descriptor, int backlog);
/**
 * @brief Accepts a pending connection.
 * @param descriptor Listening socket.
 * @param address Peer address destination, or NULL.
 * @param length Address capacity and resulting length, or NULL with address.
 * @return New descriptor, or -1 with errno set.
 */
int accept(int descriptor, struct sockaddr* address, socklen_t* length);
/**
 * @brief Sends bytes and optionally a destination address.
 * @param descriptor Socket descriptor.
 * @param buffer Source bytes.
 * @param length Number of bytes.
 * @param flags Linux send flags.
 * @param address Destination address, or NULL for a connected socket.
 * @param address_length Destination length in bytes.
 * @return Bytes sent, or -1 with errno set.
 */
ssize_t sendto(int descriptor, const void* buffer, size_t length, int flags,
               const struct sockaddr* address, socklen_t address_length);
/**
 * @brief Disables receiving, sending, or both on a socket.
 * @param descriptor Socket descriptor.
 * @param how SHUT_RD, SHUT_WR, or SHUT_RDWR.
 * @return Zero, or -1 with errno set.
 */
int shutdown(int descriptor, int how);
/**
 * @brief Reads the local address of a socket.
 * @param descriptor Socket descriptor.
 * @param address Writable address buffer.
 * @param length Input buffer size and output address size.
 * @return Zero, or -1 with errno set.
 */
int getsockname(int descriptor, struct sockaddr* address, socklen_t* length);
/**
 * @brief Reads the peer address of a connected socket.
 * @param descriptor Socket descriptor.
 * @param address Writable address buffer.
 * @param length Input buffer size and output address size.
 * @return Zero, or -1 with errno set.
 */
int getpeername(int descriptor, struct sockaddr* address, socklen_t* length);
/**
 * @brief Creates a connected pair of sockets.
 * @param domain Address family.
 * @param type Socket type and flags.
 * @param protocol Protocol number.
 * @param descriptors Writable array of two descriptors.
 * @return Zero, or -1 with errno set.
 */
int socketpair(int domain, int type, int protocol, int* descriptors);
/**
 * @brief Sets a socket option.
 * @param descriptor Socket descriptor.
 * @param level Protocol or SOL_SOCKET.
 * @param option Linux option number.
 * @param value Option data.
 * @param length Option data size in bytes.
 * @return Zero, or -1 with errno set.
 */
int setsockopt(int descriptor, int level, int option, const void* value, socklen_t length);
/**
 * @brief Reads a socket option.
 * @param descriptor Socket descriptor.
 * @param level Protocol or SOL_SOCKET.
 * @param option Linux option number.
 * @param value Writable option buffer.
 * @param length Input buffer size and output option size.
 * @return Zero, or -1 with errno set.
 */
int getsockopt(int descriptor, int level, int option, void* value, socklen_t* length);
/**
 * @brief Receives a message and optionally its source address.
 * @param descriptor Socket descriptor.
 * @param buffer Writable message buffer.
 * @param length Buffer capacity in bytes.
 * @param flags Linux receive flags.
 * @param address Source address destination, or NULL.
 * @param address_length Address capacity and resulting length, or NULL with address.
 * @return Bytes received, or -1 with errno set.
 */
ssize_t recvfrom(int descriptor, void* buffer, size_t length, int flags,
                 struct sockaddr* address, socklen_t* address_length);
/**
 * @brief Sends a scatter/gather message.
 * @param descriptor Socket descriptor.
 * @param message Linux message header.
 * @param flags Linux send flags.
 * @return Bytes sent, or -1 with errno set.
 */
ssize_t sendmsg(int descriptor, const struct msghdr* message, int flags);
/**
 * @brief Receives a scatter/gather message.
 * @param descriptor Socket descriptor.
 * @param message Writable Linux message header.
 * @param flags Linux receive flags.
 * @return Bytes received, or -1 with errno set.
 */
ssize_t recvmsg(int descriptor, struct msghdr* message, int flags);
#endif
