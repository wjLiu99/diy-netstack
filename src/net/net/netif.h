#ifndef NETIF_H
#define NETIF_H
#include "ipaddr.h"
#include "nlist.h"
#include "fixmq.h"
#include "net_cfg.h"
#include "net_err.h"
#include "pktbuf.h"
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

struct _netif_t;
//不同类型网卡的回调函数表,完成硬件方面的初始化以及发送操作
typedef struct _netif_ops_t {
    //data给底层驱动使用，标识着不同的网卡，如id
    net_err_t (*open) (struct _netif_t *netif, void *data);
    void (*close) (struct _netif_t *netif);

    net_err_t (*xmit) (struct _netif_t *netif);
}netif_ops_t;

//链路层回调函数,给上层ip层使用，只需要传入ip地址
typedef struct _link_layer_t {
    netif_type_t type;

    net_err_t (*open) (struct _netif_t *netif);
    void (*close) (struct _netif_t *netif);
    net_err_t (*in) (struct _netif_t *netif, pktbuf_t *buf);
    net_err_t (*out) (struct _netif_t *netif, ipaddr_t *dest, pktbuf_t *buf);
} link_layer_t;
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

    const netif_ops_t *ops;
    const link_layer_t *linker_layer;
    void *ops_data;

} netif_t;

net_err_t netif_init (void);

//打开网卡
netif_t *netif_open(const char *dev_name, const netif_ops_t *ops, void *ops_data);

//设置地址
net_err_t netif_set_addr (netif_t *netif, ipaddr_t *ip, ipaddr_t *netmask, ipaddr_t *gateway);
net_err_t netif_set_hwaddr (netif_t *netif, const char *hwaddr, int len);

//设置网卡激活、非激活
net_err_t netif_set_active (netif_t *netif);
net_err_t netif_set_deactive (netif_t *netif);

//关闭网络接口
net_err_t netif_close (netif_t *netif);
//设置缺省网络接口,用于往外发送数据
void netif_set_default (netif_t *netif);

//网卡输入队列的读写,tmo没数据的时候要不要等待
net_err_t netif_put_in (netif_t *netif, pktbuf_t *buf, int tmo);
pktbuf_t *netif_get_in (netif_t *netif, int tmo);

//输出队列
net_err_t netif_put_out (netif_t *netif, pktbuf_t *buf, int tmo);
pktbuf_t *netif_get_out (netif_t *netif, int tmo);


//发送数据
net_err_t netif_out (netif_t *netif, ipaddr_t *ipaddr, pktbuf_t *buf);

//注册链路层接口
net_err_t netif_register_layer (int type, const link_layer_t *layer);
#endif