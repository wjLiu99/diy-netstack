#include "net.h"
#include "exmsg.h"
#include "net_plat.h"
#include "pktbuf.h"
#include "dbg.h"
#include "net_cfg.h"
#include "netif.h"
#include "loop.h"


net_err_t net_init(void){
    dbg_info(DBG_INIT, "net stack init ...");
    net_plat_init();
    exmsg_init();
    pktbuf_init();
    netif_init();
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