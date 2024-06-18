#ifndef NET_CFG_H
#define NET_CFG_H

#include"dbg.h"
#define DBG_MBLOCK DBG_LEVEL_INFO
#define DBG_FIXMQ  DBG_LEVEL_INFO
#define DBG_EXMSG DBG_LEVEL_INFO
#define DBG_PKTBUF DBG_LEVEL_INFO
#define DBG_INIT DBG_LEVEL_INFO
#define DBG_PLAT DBG_LEVEL_INFO
#define DBG_NETIF DBG_LEVEL_INFO
#define DBG_ETHER DBG_LEVEL_INFO
#define DBG_TOOLS DBG_LEVEL_INFO
 

#define NET_ENDIAN_LITTLE 1

#define EXMSG_MSG_CNT 100
#define EXMSG_LOCKER NLOCKER_THREAD

#define PKTBUF_BLK_SIZE 128
#define PKTBUF_BLK_CNT  200
#define PKTBUF_BUF_CNT  100


#define NETIF_HWADDR_SIZE 10
#define NETIF_NAME_SIZE 10
#define NETIF_INMQ_SIZE 50
#define NETIF_OUTMQ_SIZE 50
#define NETIF_DEV_CNT 10



#define ETHER_DATA_MIN 46
#endif