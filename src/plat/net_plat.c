#include "net_plat.h"
#include "dbg.h"
#include "net_cfg.h"
net_err_t net_plat_init (void){

    dbg_info(DBG_PLAT, "plat init .....");
    dbg_info(DBG_PLAT, "plat init done");
    return NET_ERR_OK;
}