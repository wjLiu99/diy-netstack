#ifndef TCP_OUT_H
#define TCP_OUT_H

#include "tcp.h"

//发送reset分节
net_err_t tcp_send_reset (tcp_seg_t *seg);

//发送syn分节
net_err_t tcp_send_syn (tcp_t *tcp);

//发送ack分节
net_err_t tcp_ack_process (tcp_t *tcp, tcp_seg_t *seg);
net_err_t tcp_send_ack (tcp_t *tcp, tcp_seg_t *seg);

//发送fin分节
net_err_t tcp_send_fin (tcp_t *tcp);
#endif