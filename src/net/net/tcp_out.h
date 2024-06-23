#ifndef TCP_OUT_H
#define TCP_OUT_H

#include "tcp.h"

//发送reset报文
net_err_t tcp_send_reset (tcp_seg_t *seg);
#endif