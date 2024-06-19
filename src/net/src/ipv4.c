#include "ipv4.h"
#include "dbg.h"
#include "net_cfg.h"
#include "ntools.h"


net_err_t ipv4_init (void) {
    dbg_info(DBG_IP, "ipv4 init..");

    dbg_info(DBG_IP, "ipv4 init done..");
    return NET_ERR_OK;

}

//检查ip包头合法性
static net_err_t is_pkt_ok (ipv4_pkt_t *pkt, int size, netif_t *netif) {
    if (pkt->ipv4_hdr.version != NET_VERSION_IPV4) {
        dbg_warning(DBG_IP, "ipv4 version err");
        return NET_ERR_UNSUPPORT;
    }
    int hdr_len = ipv4_hdr_size(pkt);
    if (hdr_len < sizeof(ipv4_hdr_t)) {
        dbg_warning(DBG_IP, "ipv4 hdr size err");
        return NET_ERR_SIZE;
    }

    int total_size = x_ntohs(pkt->ipv4_hdr.total_len);
    if ((total_size < sizeof(ipv4_hdr_t)) || (size < total_size)) {
         dbg_warning(DBG_IP, "ipv4 hdr size err");
        return NET_ERR_SIZE;
    }

    return NET_ERR_OK;
}

static void iphdr_ntohs (ipv4_pkt_t *pkt) {
    pkt->ipv4_hdr.total_len = x_ntohs(pkt->ipv4_hdr.total_len);
    pkt->ipv4_hdr.id = x_ntohs(pkt->ipv4_hdr.id);
    pkt->ipv4_hdr.frag_all -= x_ntohs(pkt->ipv4_hdr.frag_all);
}

net_err_t ipv4_in (netif_t *netif, pktbuf_t *buf) {
    dbg_info(DBG_IP, "ipv4 in");

    net_err_t err = pktbuf_set_cont(buf, sizeof(ipv4_hdr_t));
    if (err < 0) {
        return err;
    }

    ipv4_pkt_t *ipv4_pkt = (ipv4_pkt_t *)pktbuf_data(buf);
    if (is_pkt_ok(ipv4_pkt, buf->total_size, netif) != NET_ERR_OK) {
        return NET_ERR_NONE;
    }
    //大小端转换
    iphdr_ntohs(ipv4_pkt);
    //去除填充数据
    err = pktbuf_resize(buf, ipv4_pkt->ipv4_hdr.total_len);
    if (err < 0){
        dbg_error(DBG_IP, "ippkt resize err");
        return NET_ERR_NONE;
    }

   

    pktbuf_free(buf);
    return NET_ERR_OK;


}