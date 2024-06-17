#include "loop.h"
#include "netif.h"
#include "exmsg.h"
static net_err_t loop_open (struct _netif_t *netif, void *data){
    netif->type = NETIF_TYPE_LOOP;

    return NET_ERR_OK;
}
static void loop_close (struct _netif_t *netif){

}

static net_err_t loop_xmit (struct _netif_t *netif){

    pktbuf_t *buf = netif_get_out(netif, -1);
    if (buf) {
        net_err_t err = netif_put_in(netif, buf, -1);
        if (err < 0) {
            pktbuf_free(buf);
            return err;
        }
        
    }
    return NET_ERR_OK;
}
//环回网卡驱动接口
static const netif_ops_t loop_ops = {
    .open = loop_open,
    .close = loop_close,
    .xmit = loop_xmit,
};

net_err_t loop_init (void) {
    dbg_info(DBG_NETIF, "loop init ...");

    netif_t *netif = netif_open("loop", &loop_ops, (void *)0);
    if (!netif) {
        dbg_error(DBG_NETIF, "loop open err");
        return NET_ERR_NONE;
    }

    ipaddr_t ip, netmask;
    ipaddr_from_str(&ip, "127.0.0.1");
    ipaddr_from_str(&netmask, "255.0.0.0");

    netif_set_addr(netif, &ip, &netmask, (ipaddr_t *)0);

    netif_set_active(netif);

    // pktbuf_t *buf = pktbuf_alloc(100);
    // netif_out(netif, (ipaddr_t *)0, buf);
    dbg_info(DBG_NETIF, "loop init done");
    return NET_ERR_OK;
}