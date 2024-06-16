//交换消息，处理消息,核心工作线程
#ifndef EXMSG_H
#define EXMSG_H

#include"net_err.h"
#include "nlist.h"

//通用消息结构
typedef struct _exmsg_t{
    nlist_node_t node;
    enum{
        NET_EXMSG_NETIF_IN,     //网卡有数据到达
    }type;                      //消息类型

    int id;
}exmsg_t;

net_err_t exmsg_init(void);
net_err_t exmsg_start(void);

//网卡有数据到达，发送数据到消息队列
net_err_t exmsg_netif_in(void);

#endif