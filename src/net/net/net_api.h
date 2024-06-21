#ifndef NET_API_H
#define NET_API_H

#include "ntools.h"
#include "socket.h"

#define in_addr             x_in_addr
#define sockaddr_in         x_sockaddr_in
#define sockaddr            x_sockaddr
#define socklen_t           x_socklen_t
#define timeval             x_timeval

#define socket(family, type, protocol)              x_socket(family, type, protocol)
#define sendto(s, buf, len, flags, dest, dlen)      x_sendto(s, buf, len, flags, dest, dlen)
#define recvfrom(s, buf, len, flags, src, slen)     x_recvfrom(s, buf, len, flags, src, slen)
#define setsockopt(s, level, optname, optval, len)  x_setsockopt(s, level, optname, optval, len)

#undef htons
#define htons(x)        x_htons(x)
#undef htonl
#define htonl(x)        x_htonl(x)
#undef ntohs
#define ntohs(x)        x_ntohs(x)
#undef ntohl
#define ntohl(x)        x_ntohl(x)


char* x_inet_ntoa(struct x_in_addr in);
uint32_t x_inet_addr(const char* str);
int x_inet_pton(int family, const char *strptr, void *addrptr);
const char * x_inet_ntop(int family, const void *addrptr, char *strptr, size_t len);

#define inet_ntoa(addr)             x_inet_ntoa(addr)
#define inet_addr(str)              x_inet_addr(str)

#define inet_pton(family, strptr, addrptr)          x_inet_pton(family, strptr, addrptr)
#define inet_ntop(family, addrptr, strptr, len)     x_inet_ntop(family, addrptr, strptr, len)
#endif