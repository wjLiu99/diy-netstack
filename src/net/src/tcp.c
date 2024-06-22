#include "tcp.h"
#include "dbg.h"
#include "mblock.h"

static tcp_t tcp_tbl[UDP_MAX_NR];
static mblock_t tcp_mblock;
static nlist_t tcp_list;

net_err_t tcp_init (void) {
    dbg_info(DBG_TCP, "tcp init");
    mblock_init(&tcp_mblock, tcp_tbl, sizeof(tcp_t), TCP_MAX_NR, NLOCKER_NONE);
    nlist_init(&tcp_list);
    dbg_info(DBG_TCP, "tcp init done");
}