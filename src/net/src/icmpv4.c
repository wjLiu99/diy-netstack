#include "icmpv4.h"
#include "net_cfg.h"
#include "dbg.h"
#include "ipv4.h"

net_err_t icmpv4_init (void) {
    dbg_error(DBG_ICMP, "icmp init");


    dbg_error(DBG_ICMP, "icmp init done");
    return NET_ERR_OK;

}

static net_err_t is_pkt_ok (icmpv4_pkt_t *pkt, int size, pktbuf_t *buf) {
    if(size <= sizeof(ipv4_hdr_t)) {
        dbg_error(DBG_ICMP, "size err");
        return NET_ERR_SIZE;
    }

    uint16_t checksum = pktbuf_checksum16(buf, size, 0, 1);
    if (checksum != 0) {
        dbg_error(DBG_ICMP, "checksum err");
        return NET_ERR_CHECKSUM;
    }


    return NET_ERR_OK;
}
net_err_t icmpv4_in (ipaddr_t *src_ip, ipaddr_t *netif_ip, pktbuf_t *buf) {
    dbg_info(DBG_ICMP, "icmpv4 in");
    //不能立即移除ipv4包头
    
    ipv4_pkt_t *ipv4_pkt = (ipv4_pkt_t *)pktbuf_data(buf);
    int iphdr_size = ipv4_hdr_size(ipv4_pkt);

    net_err_t err = pktbuf_set_cont(buf, iphdr_size + sizeof(icmpv4_hdr_t));
    if (err < 0) {
        dbg_error(DBG_ICMP, "set icmp cont err");
        return err;
    }

    ipv4_pkt = (ipv4_pkt_t *)pktbuf_data(buf);

    err = pktbuf_remove_header(buf, iphdr_size);
    if (err < 0) {
        dbg_error(DBG_ICMP, "remove ip hdr err");
        return NET_ERR_NONE;
    }
    
    //校验和要重新设置数据包读写位置
    pktbuf_reset_acc(buf);
    icmpv4_pkt_t *icmp_pkt = (icmpv4_pkt_t *)pktbuf_data(buf);
    
    if (is_pkt_ok(icmp_pkt, buf->total_size, buf) != NET_ERR_OK) {
        dbg_error(DBG_ICMP, "check icmp pkt err");
        return NET_ERR_NONE;
    }
    

    return NET_ERR_OK;
}