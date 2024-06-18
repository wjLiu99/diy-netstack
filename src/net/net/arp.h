#ifndef ARP_H
#define ARP_H
#include "ipaddr.h"
#include "ether.h"
#include "pktbuf.h"

#define ARP_HW_ETHER            0x1             // 以太网类型
#define ARP_REQUEST             0x1             // ARP请求包
#define ARP_REPLY               0x2             // ARP响应包

#pragma pack(1)

//arp包结构
typedef struct _arp_pkt_t {
    uint16_t htype;         // 硬件类型
    uint16_t ptype;         // 协议类型
    uint8_t hlen;           // 硬件地址长
    uint8_t plen;           // 协议地址长
    uint16_t opcode;        // 请求/响应
    uint8_t send_haddr[ETHER_HWA_SIZE];       // 发送包硬件地址
    uint8_t send_paddr[IPV4_ADDR_SIZE];     // 发送包协议地址
    uint8_t target_haddr[ETHER_HWA_SIZE];     // 接收方硬件地址
    uint8_t target_paddr[IPV4_ADDR_SIZE];   // 接收方协议地址
}arp_pkt_t;

#pragma pack()

//arp缓存表项
typedef struct _arp_entry_t {

    uint8_t paddr[IPV4_ADDR_SIZE];
    uint8_t hwaddr[ETHER_HWA_SIZE];
    nlist_node_t node;

    enum {
        NET_ARP_FREE,
        NET_ARP_WAITING,
        NET_ARP_RESOLVED,
    } state;
    nlist_t buf_list;       //pktbuf缓存

    int tmo;
    int retry;

    netif_t *netif;         //硬件地址查询成功后发送到哪一块网卡
}arp_entry_t;

net_err_t arp_init (void);

//发送arp请求包
net_err_t arp_make_request (netif_t *netif, const ipaddr_t *dest);
//发送免费arp包
net_err_t arp_make_gratuitous (netif_t *netif);

net_err_t arp_in (netif_t *netif, pktbuf_t *buf);
#endif