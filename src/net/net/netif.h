#ifndef NETIF_H
#define DNTIF_H
#include "ipaddr.h"
#include "nlist.h"
#include "fixmq.h"
#include "net_cfg.h"
#include "net_err.h"
//硬件地址
typedef struct _netif_hwaddr_t {
    uint8_t addr[NETIF_HWADDR_SIZE];
    uint8_t len;
}netif_hwaddr_t;

//网络类型
typedef enum _netif_type_t {
    NETIF_TYPE_NONE = 0,
    NETIF_TYPE_ETHER,
    NETIF_TYPE_LOOP,
    NETIF_TYPE_SIZE,
}netif_type_t;

//网络接口结构体
typedef struct _netif_t{
    char name[NETIF_NAME_SIZE];
    netif_hwaddr_t hwaddr;

    ipaddr_t ipaddr;
    ipaddr_t netmask;
    ipaddr_t gateway;

    netif_type_t type;
    int mtu;

    nlist_node_t node;

    fixmq_t in_mq;
    void *in_mq_buf[NETIF_INMQ_SIZE];
    fixmq_t out_mq;
    void *out_mq_buf[NETIF_OUTMQ_SIZE];

    enum {
        NETIF_CLOSED,
        NETIF_OPENED,
        NETIF_ACTIVE,

    } state;

} netif_t;

net_err_t netif_init (void);

//打开网卡
netif_t *netif_open(const char *dev_name);

#endif