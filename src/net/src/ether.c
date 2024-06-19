#include "ether.h"
#include "netif.h"
#include "dbg.h"
#include "protocol.h"
#include "ntools.h"
#include "arp.h"
#include "ipv4.h"

#if DBG_DISPLAY_ENABLED(DBG_ETHER)
static void display_ether_pkt(char * title, ether_pkt_t * pkt, int size) {
    ether_hdr_t * hdr = (ether_hdr_t *)pkt;

    plat_printf("\n--------------- %s ------------------ \n", title);
    plat_printf("\tlen: %d bytes\n", size);
    dbg_dump_hwaddr("\tdest:", hdr->dest, ETHER_HWA_SIZE);
    dbg_dump_hwaddr("\tsrc:", hdr->src, ETHER_HWA_SIZE);
    plat_printf("\ttype: %04x - ", x_ntohs(hdr->protocol));
    switch (x_ntohs(hdr->protocol)) {
    case NET_PROTOCOL_ARP:
        plat_printf("ARP\n");
        break;
    case NET_PROTOCOL_IPv4:
        plat_printf("IP\n");
        break;
    default:
        plat_printf("Unknown\n");
        break;
    }
    plat_printf("\n");
}

#else
#define display_ether_pkt(title, pkt, size)
#endif


net_err_t ether_open (struct _netif_t *netif){
    return arp_make_gratuitous(netif);
}
void ether_close (struct _netif_t *netif){
    //清空arp缓存
    arp_clear(netif);

}
static net_err_t is_pkt_ok(ether_pkt_t *frame, int total_size) {
    if (total_size > (sizeof(ether_hdr_t) + ETHER_MTU)){
        dbg_warning(DBG_ETHER, "frame size too big: %d",total_size);
        return NET_ERR_SIZE;
    }
    if (total_size < sizeof(ether_hdr_t)){
       dbg_warning(DBG_ETHER, "frame size too small: %d",total_size);
        return NET_ERR_SIZE; 
    }
    return NET_ERR_OK;
}
net_err_t ether_in (struct _netif_t *netif , pktbuf_t *buf){
    dbg_info(DBG_ETHER, "ether in");
    ether_pkt_t *pkt = (ether_pkt_t *)pktbuf_data(buf);

    net_err_t err;
    if ((err = is_pkt_ok(pkt, buf->total_size)) < 0){
        dbg_warning(DBG_ETHER, "ether pkt err");
        return err;
    }

    display_ether_pkt("ether in", pkt, buf->total_size);
    switch (x_ntohs(pkt->hdr.protocol))
    {
    case NET_PROTOCOL_ARP:
        err = pktbuf_remove_header(buf, sizeof(ether_hdr_t));
        if (err < 0) {
            dbg_error(DBG_ETHER, "remove header failed");
            return NET_ERR_NONE;
        }

        return arp_in(netif, buf);

    case NET_PROTOCOL_IPv4:
        arp_update_from_ipbuf(netif, buf);
        err = pktbuf_remove_header(buf, sizeof(ether_hdr_t));
        if (err < 0) {
            dbg_error(DBG_ETHER, "remove header failed");
            return NET_ERR_NONE;
        }

        return ipv4_in(netif, buf);
    
    default:
        dbg_warning(DBG_ETHER, "unknown packet");
        return NET_ERR_UNSUPPORT;
       
    }
    
    pktbuf_free(buf);
    return NET_ERR_OK;
}


net_err_t ether_out (struct _netif_t *netif, ipaddr_t *dest, pktbuf_t *buf){
    if (ipaddr_is_equal(&netif->ipaddr, dest)){
        return ether_raw_out(netif, NET_PROTOCOL_IPv4, netif->hwaddr.addr, buf);
    }

    //效率低，查找两次arp表，resolve也回查找
    const uint8_t *hwaddr = arp_find(netif, dest);
    if (hwaddr) {
        return ether_raw_out(netif, NET_PROTOCOL_IPv4, hwaddr, buf);
    } else {
        //查arp缓存表，内部会解决发送
        return arp_resolve(netif, dest, buf);
    }
    
    
    

}

static const link_layer_t ether_link_layer = {
    .type = NETIF_TYPE_ETHER,
    .open = ether_open,
    .close = ether_close,
    .in = ether_in,
    .out = ether_out,
};

net_err_t ether_init(void) {
    dbg_info(DBG_ETHER, "ether init ...");
    net_err_t err = netif_register_layer(NETIF_TYPE_ETHER, &ether_link_layer);
    if (err < 0){
        dbg_error(DBG_ETHER, "reg err");
        return err;
    }
    dbg_info(DBG_ETHER, "ether init done");

    return NET_ERR_OK;
}

const uint8_t *ether_broadcast_addr(void) {
    static const uint8_t broadcast[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    return broadcast;
}

net_err_t ether_raw_out (netif_t *netif, uint16_t protocol, const uint8_t *dest, pktbuf_t *buf) {
    int size = pktbuf_total(buf);
    net_err_t err = 0;
    if (size < ETHER_DATA_MIN) {
        dbg_info(DBG_ETHER, "resize from %d to %d", size, ETHER_DATA_MIN);
        err = pktbuf_resize(buf, ETHER_DATA_MIN);
        if (err < 0) {
            dbg_error(DBG_ETHER, "resize err");
            return err;
        }

        pktbuf_reset_acc(buf);
        pktbuf_seek(buf, size);
        pktbuf_fill(buf, 0, ETHER_DATA_MIN - size);
        size = ETHER_DATA_MIN;
    }

    err = pktbuf_add_header(buf, sizeof(ether_hdr_t), 1);
    if (err < 0){
        dbg_error(DBG_ETHER, "add ether header err");
        return err;
    }

    ether_pkt_t *pkt = (ether_pkt_t *)pktbuf_data(buf);
    plat_memcpy(pkt->hdr.dest, dest, ETHER_HWA_SIZE);
    plat_memcpy(pkt->hdr.src, netif->hwaddr.addr, ETHER_HWA_SIZE);
    pkt->hdr.protocol = x_htons(protocol);
    display_ether_pkt("ether out", pkt, size);

    //如果目的地址与网卡硬件地址相同，直接写到输入队列
    if (plat_memcmp(netif->hwaddr.addr, dest, ETHER_HWA_SIZE) == 0) {
        return netif_put_in(netif, buf, -1);
    }else {
        err = netif_put_out(netif, buf, -1);
        if (err < 0) {
            dbg_warning(DBG_ETHER, "put pkt out failed");
            return err;
        }

        return netif->ops->xmit(netif);
    }
   

}