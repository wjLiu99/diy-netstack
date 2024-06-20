//交换消息，处理消息,核心工作线程
#ifndef EXMSG_H
#define EXMSG_H

#include "net_err.h"
#include "nlist.h"
#include "netif.h"
struct _func_msg_t;
typedef net_err_t (*exmsg_func_t) (struct _func_msg_t *msg);
typedef struct _msg_netif_t {
    netif_t *netif;
} msg_netif_t;

typedef struct _func_msg_t {
    sys_thread_t thread;
    exmsg_func_t func;
    void *param;
    net_err_t err;
    sys_sem_t wait_sem;
} func_msg_t;


//通用消息结构
typedef struct _exmsg_t{
    nlist_node_t node;
    enum{
        NET_EXMSG_NETIF_IN,     //网卡有数据到达
        NET_EXMSG_FUNC,
    }type;                      //消息类型

    union 
    {
        msg_netif_t netif;
        func_msg_t *func;
    };
    
    
}exmsg_t;

net_err_t exmsg_init(void);
net_err_t exmsg_start(void);

//网卡有数据到达，发送数据到消息队列
net_err_t exmsg_netif_in(netif_t *netif);

net_err_t exmsg_func_exec (exmsg_func_t func, void *param);

#endif