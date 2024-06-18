#include "arp.h"
#include "dbg.h"
#include "net_cfg.h"
#include "mblock.h"
#include "ntools.h"
#include "protocol.h"

static arp_entry_t cache_tbl[ARP_CACHE_SIZE];
static mblock_t cache_mblock;
static nlist_t cache_list;


#if DBG_DISPLAY_ENABLED(DBG_ARP)


//打印已解析的arp表项
void display_arp_entry(arp_entry_t *entry) {
    plat_printf("%d: ", (int)(entry - cache_tbl));       // 序号
    dbg_dump_ip_buf(" ip:", entry->paddr);
    dbg_dump_hwaddr(" mac:", entry->hwaddr, ETH_HWA_SIZE);
    plat_printf(" tmo: %d, retry: %d, %s, buf: %d\n",
        entry->tmo, entry->retry, entry->state == NET_ARP_RESOLVED ? "stable" : "pending",
        nlist_count(&entry->buf_list));
}


//显示ARP表中所有项
 void display_arp_tbl(void) {
    plat_printf("\n------------- ARP table start ---------- \n");

    arp_entry_t* entry = cache_tbl;
    for (int i = 0; i < ARP_CACHE_SIZE; i++, entry++) {
        if ((entry->state != NET_ARP_FREE)) {
            continue;
        }

        display_arp_entry(entry);
    }

    plat_printf("------------- ARP table end ---------- \n");
}


//打印ARP包的完整类型
static void arp_pkt_display(arp_pkt_t* packet) {
    uint16_t opcode = x_ntohs(packet->opcode);

    plat_printf("--------------- arp start ------------------\n");
    plat_printf("    htype:%x\n", x_ntohs(packet->htype));
    plat_printf("    pype:%04x\n", x_ntohs(packet->ptype));
    plat_printf("    hlen: %x\n", packet->hlen);
    plat_printf("    plen:%x\n", packet->plen);
    plat_printf("    type:%04x  ", opcode);
    switch (opcode) {
    case ARP_REQUEST:
        plat_printf("request\n");
        break;;
    case ARP_REPLY:
        plat_printf("reply\n");
        break;
    default:
        plat_printf("unknown\n");
        break;
    }
    dbg_dump_ip_buf("    sender:", packet->send_paddr);
    dbg_dump_hwaddr("  mac:", packet->send_haddr, ETH_HWA_SIZE);
    plat_printf("\n");
    dbg_dump_ip_buf("    target:", packet->target_paddr);
    dbg_dump_hwaddr("  mac:", packet->target_haddr, ETH_HWA_SIZE);
    plat_printf("\n");
    plat_printf("--------------- arp end ------------------ \n");
}

#else
#define display_arp_entry(entry)
#define display_arp_tbl()
#define arp_pkt_display(packet)
#endif


static net_err_t cache_init (void) {
    nlist_init(&cache_list);
    net_err_t err = mblock_init(&cache_mblock, cache_tbl, sizeof(arp_entry_t), ARP_CACHE_SIZE, NLOCKER_NONE);
    if (err < 0) {
        return err;
    }
    return NET_ERR_OK;
}

net_err_t arp_init (void) {
    net_err_t err = cache_init();
    if(err < 0) {
        dbg_error(DBG_ARP, "arp cache init failed");
        return err;
    }

    return NET_ERR_OK;
}

net_err_t arp_make_request (netif_t *netif, const ipaddr_t *dest) {
    pktbuf_t *buf = pktbuf_alloc(sizeof(arp_pkt_t));
    if (buf == (pktbuf_t *)0) {
        dbg_error(DBG_ARP, "alloc pktbuf err");
        return NET_ERR_FULL;

    }

    pktbuf_set_cont(buf, sizeof(arp_pkt_t));
    arp_pkt_t *arp_pkt = (arp_pkt_t *)pktbuf_data(buf);
    //两字节要大小端转换
    arp_pkt->htype = x_htons(ARP_HW_ETHER);
    arp_pkt->ptype = x_htons(NET_PROTOCOL_IPv4);
    arp_pkt->hlen = ETHER_HWA_SIZE;
    arp_pkt->plen = IPV4_ADDR_SIZE;
    arp_pkt->opcode = x_htons(ARP_REQUEST);
    plat_memcpy(arp_pkt->send_haddr, netif->hwaddr.addr, ETH_HWA_SIZE);
    ipaddr_to_buf(&netif->ipaddr, arp_pkt->send_paddr);
    plat_memset(arp_pkt->target_haddr, 0, ETH_HWA_SIZE);
    ipaddr_to_buf(dest, arp_pkt->target_paddr);
    arp_pkt_display(arp_pkt);

    //不能调用链路层的发送了
    net_err_t err = ether_raw_out(netif, NET_PROTOCOL_ARP, ether_broadcast_addr(), buf);
    if (err < 0) {
        pktbuf_free(buf);
    }
    return err;

}

net_err_t arp_make_gratuitous (netif_t *netif) {
    dbg_info(DBG_ARP, "arp make gratuitous");
    return arp_make_request(netif, &netif->ipaddr);
}

static net_err_t is_pkt_ok (arp_pkt_t *arp_packet, uint16_t size, netif_t *netif) {
    if (size < sizeof(arp_pkt_t)) {
        dbg_warning(DBG_ARP, "packet size error: %d < %d", size, (int)sizeof(arp_pkt_t));
        return NET_ERR_SIZE;
    }

    // 上层协议和硬件类型不同的要丢掉
    if ((x_ntohs(arp_packet->htype) != ARP_HW_ETHER) ||
        (arp_packet->hlen != ETH_HWA_SIZE) ||
        (x_ntohs(arp_packet->ptype) != NET_PROTOCOL_IPv4) ||
        (arp_packet->plen != IPV4_ADDR_SIZE)) {
        dbg_warning(DBG_ARP, "packet incorrect");
        return NET_ERR_UNSUPPORT;
    }

    // 可能还有RARP等类型，全部丢掉
    uint32_t opcode = x_ntohs(arp_packet->opcode);
    if ((opcode != ARP_REQUEST) && (opcode != ARP_REPLY)) {
        dbg_warning(DBG_ARP, "unknown opcode=%d", arp_packet->opcode);
        return NET_ERR_UNSUPPORT;
    }

    return NET_ERR_OK;
}

net_err_t arp_make_reply (netif_t *netif, pktbuf_t *buf) {
    arp_pkt_t *arp_pkt = (arp_pkt_t *)pktbuf_data(buf);

    arp_pkt->opcode = x_htons(ARP_REPLY);

    plat_memcpy(arp_pkt->target_haddr, arp_pkt->send_haddr, ETH_HWA_SIZE);
    plat_memcpy(arp_pkt->target_paddr, arp_pkt->send_paddr, IPV4_ADDR_SIZE);
    plat_memcpy(arp_pkt->send_haddr, netif->hwaddr.addr, ETH_HWA_SIZE);
    ipaddr_to_buf(&netif->ipaddr, arp_pkt->send_paddr);
    arp_pkt_display(arp_pkt);

    return ether_raw_out(netif, NET_PROTOCOL_ARP, arp_pkt->target_haddr, buf);
    
}
net_err_t arp_in (netif_t *netif, pktbuf_t *buf) {
    dbg_info(DBG_ARP, "arp in");
    net_err_t err = pktbuf_set_cont(buf, sizeof(arp_pkt_t));
    if (err < 0) {
        return err;
    }

    arp_pkt_t *arp_pkt = (arp_pkt_t *)pktbuf_data(buf);
    if (is_pkt_ok(arp_pkt, buf->total_size, netif) != NET_ERR_OK) {
        return NET_ERR_NONE;
    }

    //如果是arp请求
    if (x_ntohs(arp_pkt->opcode) == ARP_REQUEST) {
        dbg_info(DBG_ARP, "arp request, send reply");
        return arp_make_reply(netif, buf);
    }


    pktbuf_free(buf);
    return NET_ERR_OK;


}