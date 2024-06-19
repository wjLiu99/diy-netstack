#include "ipv4.h"
#include "dbg.h"
#include "net_cfg.h"
#include "ntools.h"
#include "protocol.h"

#if DBG_DISPLAY_ENABLED(DBG_IP)
static void display_ip_packet(ipv4_pkt_t* pkt) {
    ipv4_hdr_t* ip_hdr = (ipv4_hdr_t*)&pkt->ipv4_hdr;

    plat_printf("--------------- ip ------------------ \n");
    plat_printf("    Version:%d\n", ip_hdr->version);
    plat_printf("    Header len:%d bytes\n", ipv4_hdr_size(pkt));
    plat_printf("    Totoal len: %d bytes\n", ip_hdr->total_len);
    plat_printf("    Id:%d\n", ip_hdr->id);

    plat_printf("    TTL: %d\n", ip_hdr->ttl);
    plat_printf("    Protocol: %d\n", ip_hdr->protocol);
    plat_printf("    Header checksum: 0x%04x\n", ip_hdr->hdr_checksum);
    dbg_dump_ip_buf("    src ip:", ip_hdr->dest_ip);
    plat_printf("\n");
    dbg_dump_ip_buf("    dest ip:", ip_hdr->src_ip);
    plat_printf("\n");
    plat_printf("--------------- ip end ------------------ \n");

}

#else
#define display_ip_packet(pkt)

#endif
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

    if (pkt->ipv4_hdr.hdr_checksum) {
    uint16_t c = checksum16(pkt, hdr_len, 0, 1);
    if (c != 0) {
        dbg_warning(DBG_IP, "bad checksum");
        return NET_ERR_NONE;
    }
}

    return NET_ERR_OK;
}

static void iphdr_ntohs (ipv4_pkt_t *pkt) {
    pkt->ipv4_hdr.total_len = x_ntohs(pkt->ipv4_hdr.total_len);
    pkt->ipv4_hdr.id = x_ntohs(pkt->ipv4_hdr.id);
    pkt->ipv4_hdr.frag_all -= x_ntohs(pkt->ipv4_hdr.frag_all);
}
static void iphdr_htons (ipv4_pkt_t *pkt) {
    pkt->ipv4_hdr.total_len = x_htons(pkt->ipv4_hdr.total_len);
    pkt->ipv4_hdr.id = x_htons(pkt->ipv4_hdr.id);
    pkt->ipv4_hdr.frag_all -= x_htons(pkt->ipv4_hdr.frag_all);
}


static net_err_t ip_normal_in (netif_t *netif, pktbuf_t *buf, ipaddr_t *src_ip, ipaddr_t *dest_ip) {
    ipv4_pkt_t *pkt = (ipv4_pkt_t *)pktbuf_data(buf);
    display_ip_packet(pkt);

    switch (pkt->ipv4_hdr.protocol)
    {
    case NET_PROTOCOL_ICMP:
        
        break;

    case NET_PROTOCOL_UDP:
        break;
    
    case NET_PROTOCOL_TCP:
        break;
    
    default:
        dbg_error(DBG_IP, "unknown protocol");
        break;
    }
    return NET_ERR_OK;
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

    //检查地址是否是发送给本机
    ipaddr_t dest_ip, src_ip;
    ipaddr_from_buf(&dest_ip, ipv4_pkt->ipv4_hdr.dest_ip);
    ipaddr_from_buf(&src_ip, ipv4_pkt->ipv4_hdr.src_ip);
    if (!ipaddr_is_match(&dest_ip, &netif->ipaddr, &netif->netmask)) {
        dbg_error(DBG_IP, "ipaddr not match");
        return NET_ERR_UNREACH;

    }
   
   err = ip_normal_in(netif, buf, &src_ip, &dest_ip);

    pktbuf_free(buf);
    return NET_ERR_OK;


}
static int id = 0;
net_err_t ipv4_out (uint8_t protocol, ipaddr_t *dest, ipaddr_t *src, pktbuf_t *buf) {
    dbg_info(DBG_IP, "send a ip pkt");
    net_err_t err = pktbuf_add_header(buf, sizeof(ipv4_hdr_t), 1);
    if (err < 0) {
        dbg_error(DBG_IP, "add iphdr err");
        return err;
    }
    
    //填充包头
    ipv4_pkt_t *pkt = (ipv4_pkt_t *)pktbuf_data(buf);
    pkt->ipv4_hdr.shdr_all = 0;
    pkt->ipv4_hdr.version = NET_VERSION_IPV4;
    ipv4_set_hdr_size(pkt, sizeof(ipv4_hdr_t));
    pkt->ipv4_hdr.total_len = buf->total_size;
    pkt->ipv4_hdr.id = id++;
    pkt->ipv4_hdr.frag_all = 0;
    pkt->ipv4_hdr.ttl = NET_IP_DEFAULT_TTL;
    pkt->ipv4_hdr.hdr_checksum = 0;
    ipaddr_to_buf(src, pkt->ipv4_hdr.src_ip);
    ipaddr_to_buf(dest, pkt->ipv4_hdr.dest_ip);

    //转换成大端发送
    iphdr_htons(pkt);
    display_ip_packet(pkt);
    pktbuf_reset_acc(buf);
    pkt->ipv4_hdr.hdr_checksum = pktbuf_checksum16(buf, ipv4_hdr_size(pkt), 0, 1);

    //发送
    err = netif_out(netif_get_default(), dest, buf);
    if (err < 0) {
        dbg_error(DBG_IP, "send ip pkt err");
        return err;
    }

    return NET_ERR_OK;

}