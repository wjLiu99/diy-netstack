#include "ipv4.h"
#include "dbg.h"
#include "net_cfg.h"
#include "ntools.h"
#include "protocol.h"
#include "icmpv4.h"
#include "mblock.h"
#include "net_cfg.h"
#include "raw.h"

static ip_frag_t frag_array[IP_FRAGS_MAX_NR];
static mblock_t frag_mblock;
static nlist_t frag_list;

static uint16_t get_frag_start (ipv4_pkt_t *pkt) {
    return pkt->ipv4_hdr.offset * 8;
}

static inline int get_data_size(ipv4_pkt_t* pkt) {
    return pkt->ipv4_hdr.total_len - ipv4_hdr_size(pkt);
}

static uint16_t get_frag_end (ipv4_pkt_t *pkt) {
    return get_frag_start(pkt) + get_data_size(pkt);
}
#if DBG_DISPLAY_ENABLED(DBG_IP)
static void display_ip_frags(void) {
    plat_printf("==========ip frag=============\n");
    nlist_node_t *f_node, * p_node;
    int f_index = 0, p_index = 0;

    plat_printf("DBG_IP frags:");
    for (f_node = nlist_first(&frag_list); f_node; f_node = nlist_node_next(f_node)) {
        ip_frag_t* frag = nlist_entry(f_node, ip_frag_t, node);
        plat_printf("[%d]:\n", f_index++);
        dbg_dump_ip_buf("\tip:", frag->ip.a_addr);
        plat_printf("\tid: %d\n", frag->id);
        plat_printf("\ttmo: %d\n", frag->tmo);
        plat_printf("\tbufs: %d\n", nlist_count(&frag->buf_list));

        // 逐个显示各个分片的buf
        plat_printf("\tbufs:\n");
        nlist_for_each(p_node, &frag->buf_list) {
            pktbuf_t * buf = nlist_entry(p_node, pktbuf_t, node);
            ipv4_pkt_t* pkt = (ipv4_pkt_t *)pktbuf_data(buf);

            plat_printf("\t\tB%d[%d - %d], ", p_index++, get_frag_start(pkt), get_frag_end(pkt) - 1);
        }
        plat_printf("\n");
}
    plat_printf("===========ip frag end============\n");
}

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
    dbg_dump_ip_buf("    src ip:", ip_hdr->src_ip);
    plat_printf("\n");
    dbg_dump_ip_buf("    dest ip:", ip_hdr->dest_ip);
    plat_printf("\n");
    plat_printf("--------------- ip end ------------------ \n");

}

#else
#define display_ip_packet(pkt)
#define display_ip_frags()
#endif


static net_err_t frag_init (void) {
    nlist_init(&frag_list);
    net_err_t err = mblock_init(&frag_mblock, frag_array, sizeof(ip_frag_t), IP_FRAGS_MAX_NR, NLOCKER_NONE);
    if (err < 0) {
        dbg_error(DBG_IP, "frag mblock init err");
        return err;
    }
    return NET_ERR_OK;
}

static void frag_free_buf_list (ip_frag_t *frag) {
    nlist_node_t * node;
    while ((node = nlist_remove_first(&frag->buf_list))) {
        pktbuf_t *buf = nlist_entry(node, pktbuf_t, node);
        pktbuf_free(buf);
    }
}

static ip_frag_t * frag_alloc (void) {
    ip_frag_t *frag = mblock_alloc(&frag_mblock, -1);
    //分片满了删除最后的分片
    if (!frag) {
        nlist_node_t *node = nlist_remove_last(&frag_list);
        frag = nlist_entry(node, ip_frag_t, node);
        if (frag) {
            frag_free_buf_list(frag);
        }
    }
    return frag;
}

static void frag_free (ip_frag_t *frag) {
    frag_free_buf_list(frag);
    nlist_remove(&frag_list, &frag->node);
    mblock_free(&frag_mblock, frag);
}
net_err_t ipv4_init (void) {
    dbg_info(DBG_IP, "ipv4 init..");
    net_err_t err = frag_init();
    if (err < 0) {
        dbg_error(DBG_IP, "frag init err");
        return err;
    }
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
    pkt->ipv4_hdr.frag_all = x_ntohs(pkt->ipv4_hdr.frag_all);
}
static void iphdr_htons (ipv4_pkt_t *pkt) {
    pkt->ipv4_hdr.total_len = x_htons(pkt->ipv4_hdr.total_len);
    pkt->ipv4_hdr.id = x_htons(pkt->ipv4_hdr.id);
    pkt->ipv4_hdr.frag_all = x_htons(pkt->ipv4_hdr.frag_all);
}

static ip_frag_t *frag_find (ipaddr_t *ip, uint16_t id) {
    nlist_node_t *cur;
    nlist_for_each(cur, &frag_list) {
        ip_frag_t *frag = nlist_entry(cur, ip_frag_t, node);
        if (ipaddr_is_equal(ip, &frag->ip) && (id == frag->id)) {
            //找到分片结构，插入队头提高效率，因为分片可能是批量到达
            nlist_remove(&frag_list, cur);
            nlist_insert_first(&frag_list, cur);
            return frag;
        }
    }
    return (ip_frag_t *)0;
}

static void frag_add (ip_frag_t *frag, ipaddr_t *ip, uint16_t id) {
    ipaddr_copy(&frag->ip, ip);
    frag->tmo = 0;
    frag->id = id;
    nlist_node_init(&frag->node);
    nlist_init(&frag->buf_list);
    nlist_insert_first(&frag_list, &frag->node);
    
}

static net_err_t frag_buf_insert (ip_frag_t *frag, pktbuf_t *buf, ipv4_pkt_t *pkt) {
    //限制每个分片的数据包数量
    if (nlist_count(&frag->buf_list) >= IP_FRAG_MAX_BUF_NR) {
        dbg_error(DBG_IP, "frag buf limted");
        frag_free(frag);
        return NET_ERR_BUF;
    }

    nlist_node_t *node;
    nlist_for_each(node, &frag->buf_list) {
        pktbuf_t *cur_buf = nlist_entry(node, pktbuf_t, node);
        ipv4_pkt_t *cur_pkt = (ipv4_pkt_t *)pktbuf_data(cur_buf);

        uint16_t cur_start = get_frag_start(cur_pkt);
        if (get_frag_start(pkt) == cur_start) {
            return NET_ERR_EXIST;
        } else if (get_frag_end(pkt) <= cur_start) {
           nlist_node_t *pre = nlist_node_pre(node);
           if (pre) {
            nlist_insert_after(&frag->buf_list, pre, &buf->node);
           } else {
            nlist_insert_first(&frag->buf_list, &buf->node);
           }
           return NET_ERR_OK;
        }
    }

    nlist_insert_last(&frag->buf_list, &buf->node);
    return NET_ERR_OK;
}
//检查分片数据包是否全部到达
static int frag_is_all_arrived (ip_frag_t *frag) {
    int offset = 0;
    ipv4_pkt_t *pkt = (ipv4_pkt_t *)0;
    nlist_node_t *node;
    nlist_for_each(node, &frag->buf_list) {
        pktbuf_t *buf = nlist_entry(node, pktbuf_t, node);

        pkt = (ipv4_pkt_t *)pktbuf_data(buf);
        int cur_offset = get_frag_start(pkt);
        if (offset != cur_offset) {
            return 0;
        }

        offset += get_data_size(pkt);
    }

    return pkt ? !pkt->ipv4_hdr.more : 0;
}
//ip分片重组
static pktbuf_t *frag_join (ip_frag_t *frag) {
    pktbuf_t *buf = (pktbuf_t *)0;
    nlist_node_t *node;

    while((node = nlist_remove_first(&frag->buf_list))) {
        pktbuf_t *cur = nlist_entry(node, pktbuf_t, node);

        if (!buf) {
            buf = cur;
            continue;
        }
        ipv4_pkt_t *pkt = (ipv4_pkt_t *)pktbuf_data(cur);
        net_err_t err = pktbuf_remove_header(cur, ipv4_hdr_size(pkt));
        if (err < 0) {
            dbg_error(DBG_IP, "frag remove hdr err");
            pktbuf_free(cur);
            goto free_return;
        }
        err = pktbuf_join(buf, cur);
        if (err < 0) {
            dbg_error(DBG_IP, "join ip frag failed");
            pktbuf_free(cur);
            goto free_return;
        }
    }
    frag_free(frag);
    return buf;
free_return:
    if (buf) {
        pktbuf_free(buf);
    }
    frag_free(frag);
    return (pktbuf_t *)0;
}

//处理完整ip数据包
static net_err_t ip_normal_in (netif_t *netif, pktbuf_t *buf, ipaddr_t *src_ip, ipaddr_t *dest_ip) {
    ipv4_pkt_t *pkt = (ipv4_pkt_t *)pktbuf_data(buf);
    display_ip_packet(pkt);
    

    switch (pkt->ipv4_hdr.protocol)
    {
        case NET_PROTOCOL_ICMPv4:{
            net_err_t err = icmpv4_in(src_ip, &netif->ipaddr, buf);
            if (err < 0) {
                dbg_warning(DBG_IP, "icmp in failed");
                return err;
            }
            
            break;
        }

        case NET_PROTOCOL_UDP:
            iphdr_htons(pkt);
            icmpv4_out_unreach(src_ip, &netif->ipaddr, ICMPv4_PORT_UNREACH, buf);
            break;
        
        case NET_PROTOCOL_TCP:
            break;
        
        default:{
            dbg_warning(DBG_IP, "unknown protocol");
            net_err_t err = raw_in(buf);
            if (err < 0) {
                dbg_warning(DBG_IP, "raw in err");
                return err;
            }
            break;
        }
      
    }
    return NET_ERR_OK;
}

//处理分片的数据包
static net_err_t ip_frag_in (netif_t *netif, pktbuf_t *buf, ipaddr_t *src_ip, ipaddr_t *dest_ip) {
    ipv4_pkt_t *cur = (ipv4_pkt_t *)pktbuf_data(buf);
    ip_frag_t *frag = frag_find(src_ip, cur->ipv4_hdr.id);
    if (!frag) {
        frag = frag_alloc();
        frag_add(frag, src_ip, cur->ipv4_hdr.id);
    }
    net_err_t err = frag_buf_insert(frag, buf, cur);
    if (err < 0) {
        dbg_warning(DBG_IP, "frag buf insert err");
        return err;
    }
    if (frag_is_all_arrived(frag)) {
        pktbuf_t *buf = frag_join(frag);
        if (!buf) {
            dbg_error(DBG_IP, "join ip bufs failed");
            //有问题
            return NET_ERR_OK;
        }

        err = ip_normal_in(netif, buf, src_ip, dest_ip);
        if (err < 0) {
            dbg_warning(DBG_IP, "ip frag in failed");
            pktbuf_free(buf);
            return NET_ERR_OK;
        }
    }
    display_ip_frags();
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

    //检查地址是否是发送给本机,源ip是对端，目的ip是本机
    ipaddr_t dest_ip, src_ip;
    ipaddr_from_buf(&dest_ip, ipv4_pkt->ipv4_hdr.dest_ip);
    ipaddr_from_buf(&src_ip, ipv4_pkt->ipv4_hdr.src_ip);
    if (!ipaddr_is_match(&dest_ip, &netif->ipaddr, &netif->netmask)) {
        dbg_error(DBG_IP, "ipaddr not match");
        return NET_ERR_UNREACH;

    }
    //分片的数据包这两个至少有一个是不为0的
    if (ipv4_pkt->ipv4_hdr.offset || ipv4_pkt->ipv4_hdr.more) {
        err = ip_frag_in(netif, buf, &src_ip, &dest_ip);
    } else {
        err = ip_normal_in(netif, buf, &src_ip, &dest_ip);
    }


    //不用这里释放，返回错误才释放，交给下层就认为包给下层管理了
    // pktbuf_free(buf);
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
    pkt->ipv4_hdr.protocol = protocol;
    pkt->ipv4_hdr.hdr_checksum = 0;
    ipaddr_to_buf(src, pkt->ipv4_hdr.src_ip);
    ipaddr_to_buf(dest, pkt->ipv4_hdr.dest_ip);
    display_ip_packet(pkt);

    //转换成大端发送
    iphdr_htons(pkt);
    
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