#include "ether.h"
#include "netif.h"
#include "dbg.h"
#include "protocol.h"
#include "ntools.h"

#if DBG_DISPLAY_ENABLED(DBG_ETHER)
static void display_ether_display(char * title, ether_pkt_t * pkt, int size) {
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
#define display_ether_display(title, pkt, size)
#endif


net_err_t ether_open (struct _netif_t *netif){
    return NET_ERR_OK;
}
void ether_close (struct _netif_t *netif){

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
    display_ether_display("ether in", pkt, buf->total_size);
    pktbuf_free(buf);
    return NET_ERR_OK;
}
net_err_t ether_out (struct _netif_t *netif, ipaddr_t *dest, pktbuf_t *buf){
    return NET_ERR_OK;
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
    
}