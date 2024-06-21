#ifndef RAW_H
#define RAW_H

#include "sock.h"
#include "pktbuf.h"
#include "nlist.h"

typedef struct _raw_t {
    sock_t base;
    //接受等待， 发送不需要等
    sock_wait_t recv_wait;

    nlist_t recv_list;  //接收队列
} raw_t;

net_err_t raw_init (void);

sock_t *raw_create (int family, int protocol);

net_err_t raw_in (pktbuf_t *buf);
#endif