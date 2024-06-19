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
#define DBG_IP                  DBG_LEVEL_INFO
#define DBG_ICMP                DBG_LEVEL_INFO
 

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
#endif