#include "net.h"
#include "exmsg.h"
#include "net_plat.h"
#include "pktbuf.h"
#include "dbg.h"
#include "net_cfg.h"
#include "netif.h"
#include "loop.h"
#include "ether.h"
#include "ntools.h"
#include "ntimer.h"
#include "arp.h"
#include "ipv4.h"
#include "icmpv4.h"
#include "sock.h"
#include "raw.h"
#include "udp.h"
#include "tcp.h"


net_err_t net_init(void){
    dbg_info(DBG_INIT, "net stack init ...");
    net_plat_init();
    exmsg_init();
    tools_init();
    pktbuf_init();
    net_timer_init();
    netif_init();

    ether_init();
    arp_init();
    ipv4_init();
    icmpv4_init();
    socket_init();
    raw_init();
    udp_init();
    tcp_init();
    //要先初始化路由表
    loop_init();

    dbg_info(DBG_INIT, "net stack init done ...");
    return NET_ERR_OK;
}

net_err_t net_start(void){
    dbg_info(DBG_INIT, "net stack start ...");
    exmsg_start();
    dbg_info(DBG_INIT, "net stack working ...");
    return NET_ERR_OK;

}