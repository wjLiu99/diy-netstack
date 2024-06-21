#ifndef UDP_H
#define UDP_H

#include "sock.h"
typedef struct _udp_t {
    sock_t base;

    //接受等待
    sock_wait_t recv_wait;
    nlist_t recv_list;  //接收队列
} udp_t;

net_err_t udp_init (void);

sock_t *udp_create (int family, int protocol);

#endif