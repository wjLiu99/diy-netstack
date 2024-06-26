#ifndef NET_CFG_H
#define NET_CFG_H

#include"dbg.h"
#define DBG_MBLOCK              DBG_LEVEL_ERROR
#define DBG_FIXMQ               DBG_LEVEL_ERROR
#define DBG_EXMSG               DBG_LEVEL_ERROR
#define DBG_PKTBUF              DBG_LEVEL_ERROR
#define DBG_INIT                DBG_LEVEL_ERROR
#define DBG_PLAT                DBG_LEVEL_ERROR
#define DBG_NETIF               DBG_LEVEL_ERROR
#define DBG_ETHER               DBG_LEVEL_ERROR
#define DBG_TOOLS               DBG_LEVEL_ERROR
#define DBG_NTIMER              DBG_LEVEL_ERROR
#define DBG_ARP                 DBG_LEVEL_ERROR
#define DBG_IP                  DBG_LEVEL_ERROR
#define DBG_ICMP                DBG_LEVEL_ERROR
#define DBG_SOCKET              DBG_LEVEL_ERROR
#define DBG_RAW                 DBG_LEVEL_ERROR
#define DBG_UDP                 DBG_LEVEL_ERROR
#define DBG_TCP                 DBG_LEVEL_INFO

 

#define NET_ENDIAN_LITTLE           1

#define EXMSG_MSG_CNT           100
#define EXMSG_LOCKER            NLOCKER_THREAD

#define PKTBUF_BLK_SIZE         128
#define PKTBUF_BLK_CNT          200
#define PKTBUF_BUF_CNT          100


#define NETIF_HWADDR_SIZE       10
#define NETIF_NAME_SIZE         10
#define NETIF_INMQ_SIZE         50
#define NETIF_OUTMQ_SIZE        50
#define NETIF_DEV_CNT           10



#define ETHER_DATA_MIN          46

#define ARP_CACHE_SIZE          10
#define ETH_HWA_SIZE            6


#define ARP_MAX_PKT_WAIT        10
#define ARP_TIMER_TMO           1
#define ARP_ENTRY_PENDING_TMO   3
#define ARP_ENTRY_RETRY_CNT     5
#define ARP_ENTRY_STABLR_TMO    5


#define NET_IP_DEFAULT_TTL      64

#define IP_FRAGS_MAX_NR         10
#define IP_FRAG_MAX_BUF_NR      10


#define RAW_MAX_NR              10
#define UDP_MAX_NR              10
#define TCP_MAX_NR              10

#define SOCKET_MAX_NR       (RAW_MAX_NR + UDP_MAX_NR + TCP_MAX_NR)


#define RAW_MAX_RECV        50
#define UDP_MAX_RECV        50

#define IP_ROUTE_NR         20
#define NET_PORT_DYN_START      1024
#define NET_PORT_DYN_END        65535


#define TCP_SBUF_SIZE           4096  //tcp发送缓存大小
#define TCP_RBUF_SIZE           4096  //tcp接收 缓存大小


#define TCP_KEEPALIVE_TIME      (20 * 60 * 60)
#define TCP_KEEPALIVE_INTVL     10
#define TCP_KEEPALIVE_PROBES    10

#define NET_CLOSE_MAX_TMO       5
#endif