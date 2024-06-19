#include "icmpv4.h"
#include "net_cfg.h"
#include "dbg.h"
#include "ipv4.h"
#include "protocol.h"
#include "ntools.h"

#if DBG_DISPLAY_ENABLED(DBG_ICMP)
static void display_icmp_packet(char * title, icmpv4_pkt_t  * pkt) {
    plat_printf("--------------- %s ------------------ \n", title);
    plat_printf("type: %d\n", pkt->hdr.type);
    plat_printf("code: %d\n", pkt->hdr.code);
    plat_printf("checksum: %x\n", x_ntohs(pkt->hdr.checksum));
    plat_printf("------------------------------------- \n");
}
#else
#define display_icmp_packet(title, packet)
#endif 

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

//从那块网卡来的往那块网卡发送,只计算校验和
net_err_t icmpv4_out (ipaddr_t *dest, ipaddr_t *src, pktbuf_t *buf) {
    icmpv4_pkt_t *pkt = (icmpv4_pkt_t *)pktbuf_data(buf);
    pktbuf_reset_acc(buf);
    pkt->hdr.checksum = pktbuf_checksum16(buf, buf->total_size, 0, 1);
    display_icmp_packet("icmpv4 out", pkt);

    return ipv4_out(NET_PROTOCOL_ICMPv4, dest, src, buf);
}
//不用重新构造数据包，在源数据包上修改直接发送
static net_err_t icmpv4_echo_reply (ipaddr_t *dest, ipaddr_t *src, pktbuf_t *buf) {
    icmpv4_pkt_t *pkt = (icmpv4_pkt_t *)pktbuf_data(buf);
    
    pkt->hdr.type = ICMPv4_ECHO_REPLY;
    pkt->hdr.checksum = 0;

    return icmpv4_out(dest, src, buf);
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
    
    switch (icmp_pkt->hdr.type)
    {
    case ICMPv4_ECHO_REQUEST:{
        return icmpv4_echo_reply(src_ip, netif_ip, buf);
        
    }
    default:
        pktbuf_free(buf);
        return NET_ERR_OK;
        
    }

    return NET_ERR_OK;
}

net_err_t icmpv4_out_unreach (ipaddr_t *dest, ipaddr_t *src, uint8_t code, pktbuf_t *buf) {
    //传入的是ip数据包，包头没有移除
    int copy_size = ipv4_hdr_size((ipv4_pkt_t *)pktbuf_data(buf)) + 576;
    if (copy_size > buf->total_size)  {
        copy_size = buf->total_size;
    }

    pktbuf_t *new_buf = pktbuf_alloc(copy_size + sizeof(icmpv4_hdr_t) + 4);
    if (!new_buf) {
        dbg_warning(DBG_ICMP, "alloc buf failed");
        return NET_ERR_NONE;
    }

    icmpv4_pkt_t *pkt  = (icmpv4_pkt_t *)pktbuf_data(new_buf);
    pkt->hdr.type = ICMPv4_UNREACH;
    pkt->hdr.code = code;
    pkt->hdr.checksum = 0;
    pkt->reverse = 0;
    pktbuf_reset_acc(buf);
    pktbuf_seek(new_buf, sizeof(icmpv4_hdr_t ) + 4);
    net_err_t err = pktbuf_copy(new_buf, buf, copy_size);
    if (err < 0) {
        dbg_error(DBG_ICMP, "buf copy err");
        pktbuf_free(new_buf);
        return err;
    }

    err = icmpv4_out(dest, src, new_buf);
    if (err < 0) {
        dbg_error(DBG_ICMP, "send icmp err");
        return err;
    }
    return NET_ERR_OK;

}