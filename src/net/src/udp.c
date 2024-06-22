#include "udp.h"
#include "dbg.h"
#include "mblock.h"
#include "nlist.h"
#include "net_cfg.h"
#include "ipv4.h"
#include "socket.h"
#include "sock.h"
#include "protocol.h"

static udp_t udp_tbl[UDP_MAX_NR];
static mblock_t udp_mblock;
static nlist_t udp_list;

#if DBG_DISPLAY_ENABLED(DBG_UDP)
static void display_udp_list (void) {
    plat_printf("--- udp list\n --- ");

    int idx = 0;
    nlist_node_t * node;

    nlist_for_each(node, &udp_list) {
        udp_t * udp = (udp_t *)nlist_entry(node, sock_t, node);
        plat_printf("[%d]\n", idx++);
        dbg_dump_ip_buf("\tlocal:", (const uint8_t *)&udp->base.local_ip.a_addr);
        plat_printf("\tlocal port: %d\n", udp->base.local_port);
        dbg_dump_ip_buf("\tremote:", (const uint8_t *)&udp->base.remote_ip.a_addr);
        plat_printf("\tremote port: %d\n", udp->base.remote_port);
    }
}
static void display_udp_packet(udp_pkt_t * pkt) {
    plat_printf("UDP packet:\n");
    plat_printf("source Port:%d\n", pkt->hdr.src_port);
    plat_printf("dest Port: %d\n", pkt->hdr.dest_port);
    plat_printf("length: %d bytes\n", pkt->hdr.total_len);
    plat_printf("checksum:  %04x\n", pkt->hdr.checksum);
}
#else

#define display_udp_packet(packet)
#define display_udp_list()
#endif

net_err_t udp_init (void) {
    dbg_info(DBG_UDP, "udp init..");
    mblock_init(&udp_mblock, udp_tbl, sizeof(udp_t), UDP_MAX_NR, NLOCKER_NONE);
    nlist_init(&udp_list);
    dbg_info(DBG_UDP, "udp init done");
    return NET_ERR_OK;

}

static int is_port_used (int port) {
    nlist_node_t *node;
    nlist_for_each(node, &udp_list) {
        sock_t *sock = nlist_entry(node, sock_t, node);
        if (sock->local_port == port) {
            return 1;
        }
    }

    return 0;
}


static net_err_t alloc_port (sock_t *sock) {
    static int search_index = NET_PORT_DYN_START-1;
    for (int i = NET_PORT_DYN_START; i < NET_PORT_DYN_END; i++) {
        if (++search_index > NET_PORT_DYN_END) {
            search_index = NET_PORT_DYN_START;
        }
        if (!is_port_used(search_index)) {
            sock->local_port = search_index;
            return NET_ERR_OK;
        }
        
    }
    return NET_ERR_NONE;
}


static net_err_t udp_sendto (struct _sock_t *s, const void *buf, size_t len, int flags, 
    const struct x_sockaddr *dest, x_socklen_t dest_len, ssize_t *result_len ) {
    
  

    ipaddr_t dest_ip;
    struct x_sockaddr_in *addr = (struct x_sockaddr_in *)dest;
    ipaddr_from_buf(&dest_ip, addr->sin_addr.addr_array);
    uint16_t dport = x_ntohs(addr->sin_port);
    //如果sock绑定了地址，发送地址必须相同
    if (!ipaddr_is_any(&s->remote_ip) && !ipaddr_is_equal(&dest_ip, &s->remote_ip)) {
        dbg_error(DBG_UDP, "dest is incorrect");
        return NET_ERR_PARAM;
    }

    //发送地址要和套接字内保存的地址相同
    if (s->remote_port && (s->remote_port != dport)) {
        dbg_error(DBG_UDP, "dest is incorrect");
        return NET_ERR_PARAM;
    }
    //没有指定端口就分配一个端口
    if (!s->local_port && ((s->err = alloc_port(s)) < 0)) {
        dbg_error(DBG_UDP, "alloc port err");
        return NET_ERR_NONE;
    }
    pktbuf_t *pktbuf = pktbuf_alloc((int)len);
    if (!pktbuf) {
        dbg_error(DBG_UDP, "no pktbuf");
        return NET_ERR_MEM;
    }
    pktbuf_reset_acc(pktbuf);
    net_err_t err = pktbuf_write(pktbuf, (uint8_t *)buf, (int)len);
    if (err < 0) {
        dbg_error(DBG_UDP, "copy_data err");
        goto end;
    }
    
    //sock没有绑定地址的话本地ip可能为空，交给下层ip协议处理
    err = udp_out(&dest_ip, dport, &s->local_ip, s->local_port, pktbuf);
    if (err < 0) {
        dbg_error(DBG_UDP, "send err");
        goto end;
    }
    *result_len = (ssize_t)len;
    return NET_ERR_OK;
end:
    pktbuf_free(pktbuf);
    return err;
}


static net_err_t udp_recvfrom (struct _sock_t *s,  void *buf, size_t len, int flags, 
     struct x_sockaddr *src, x_socklen_t *src_len, ssize_t *result_len ){

    udp_t *udp = (udp_t *)s;
    nlist_node_t *first = nlist_remove_first(&udp->recv_list); 
    //输入队列中没有数据则返回需要等待
    if (!first) {
        *result_len = 0;
        return NET_ERR_WAIT;
    }
    
    //有数据则开始读
    pktbuf_t *pktbuf = (pktbuf_t *)nlist_entry(first, pktbuf_t, node);
    udp_from_t *from = (udp_from_t *)pktbuf_data(pktbuf);

    struct x_sockaddr_in *addr = (struct x_sockaddr_in *)src;
    plat_memset(addr, 0, sizeof(struct x_sockaddr_in));
    addr->sin_family = AF_INET;
    //转换成大端，读的时候会转换成小端
    addr->sin_port = x_htons(from->port);
    ipaddr_to_buf(&from->from, addr->sin_addr.addr_array);
    //移除包头剩下用户数据
    pktbuf_remove_header(pktbuf, sizeof(udp_from_t));

    int size = (pktbuf->total_size >(int)len) ? (int)len : pktbuf->total_size;
    
    pktbuf_reset_acc(pktbuf);
    net_err_t err = pktbuf_read(pktbuf, buf, size);
    if (err < 0) {
        dbg_error(DBG_UDP, "udp pkt read err");
        //把pktbuf插入输入队列已经返回正确了，这里是pktbuf最后一个阶段，只用读入用户缓冲区，全部交由该函数释放
        pktbuf_free(pktbuf);
        return err;
    }
    pktbuf_free(pktbuf);

    //返回实际读出字节数
    *result_len = size;
    return NET_ERR_OK;

}

sock_t *udp_create (int family, int protocol) {

    static const sock_ops_t udp_ops = {
        .setopt = sock_setopt,
        .sendto = udp_sendto,
        .recvfrom = udp_recvfrom,

    };
    udp_t *udp = mblock_alloc(&udp_mblock, -1);
    if (!udp) {
        dbg_error(DBG_UDP, "udp alloc err");
        return (sock_t *)0;
    }

    net_err_t err = sock_init(&udp->base, family, protocol, &udp_ops);
    if (err < 0) {
        dbg_error(DBG_UDP, "create udp failed");
        mblock_free(&udp_mblock, udp);
        return (sock_t *)0;
    }

    nlist_init(&udp->recv_list);
    udp->base.recv_wait = &udp->recv_wait;
    
    if (sock_wait_init(udp->base.recv_wait) < 0) {
        dbg_error(DBG_UDP, "init recv wait err");
        goto create_failed;
    }


    nlist_insert_last(&udp_list, &udp->base.node);


    return (sock_t *)udp;

create_failed:
    sock_uninit(&udp->base);
    return (sock_t *)0;
}

net_err_t udp_out(ipaddr_t *dest, uint16_t dport, ipaddr_t *src, uint16_t port, pktbuf_t *buf) {
    //计算伪头部校验和要源ip地址
    if (ipaddr_is_any(src)) {
        rentry_t *rt = rt_find(dest);
        if (rt == (rentry_t *)0) {
            dbg_error(DBG_UDP, "no route");
            return NET_ERR_UNREACH;

        }
        src = &rt->netif->ipaddr;
    }

    net_err_t err = pktbuf_add_header(buf, sizeof(udp_hdr_t), 1);
    if (err < 0) {
        dbg_error(DBG_UDP, "add header err");
        return NET_ERR_SIZE;
    }

    udp_hdr_t *hdr = (udp_hdr_t *)pktbuf_data(buf);
    hdr->src_port = x_htons(port);
    hdr->dest_port = x_htons(dport);
    hdr->total_len = x_htons(buf->total_size);
    hdr->checksum = 0;
    hdr->checksum = checksum_peso(buf, dest, src, NET_PROTOCOL_UDP);

    err = ipv4_out(NET_PROTOCOL_UDP, dest, src, buf);
    if (err < 0) {
        dbg_error(DBG_UDP, "send udp pkt err");
        return NET_ERR_NONE;
    }

    return NET_ERR_OK;
}

static udp_t *udp_find (ipaddr_t *src, uint16_t sport, ipaddr_t *dest, uint16_t dport) {
    //目的端口必须有，因为是对方发来的数据包
    if (!dport) {
        return (udp_t *)0;
    }
    nlist_node_t *node;
    nlist_for_each(node, &udp_list) {
        sock_t *sock = nlist_entry(node, sock_t, node);

        if (sock->local_port != dport) {
            continue;

        }
        //sock可能没有绑定远端地址和本地地址
        if(!ipaddr_is_any(&sock->local_ip) && !ipaddr_is_equal(dest, &sock->local_ip)) {
            continue;
        }

        if (!ipaddr_is_any(&sock->remote_ip) && !ipaddr_is_equal(src, &sock->remote_ip)) {
            continue;
        }

        if (sock->remote_port && (sock->remote_port != sport)) {
            continue;
        }

        return (udp_t *)sock;
    }

    return (udp_t *)0;
}

static net_err_t is_pkt_ok(udp_pkt_t * pkt, int size) {
    if ((size < sizeof(udp_hdr_t)) || (size < pkt->hdr.total_len)) {
        dbg_error(DBG_UDP, "udp packet size incorrect: %d!", size);
        return NET_ERR_SIZE;
    }

    return NET_ERR_OK;
}
net_err_t udp_in (pktbuf_t *buf, ipaddr_t *src, ipaddr_t *dest) {
    //接收的是ip数据包
    int iphdr_size = ipv4_hdr_size((ipv4_pkt_t *)pktbuf_data(buf));

    net_err_t err = pktbuf_set_cont(buf, sizeof(udp_hdr_t) + iphdr_size);
    if (err < 0) {
        dbg_error(DBG_UDP, "udp set cont err");
        return err;
    }
    
    udp_pkt_t *udp_pkt = (udp_pkt_t *)((pktbuf_data(buf)) + iphdr_size);
    uint16_t dest_port = x_ntohs(udp_pkt->hdr.dest_port);
    uint16_t src_port = x_ntohs(udp_pkt->hdr.src_port);

    udp_t *udp = (udp_t *)udp_find(src, src_port, dest, dest_port);
    if (!udp) {
        dbg_error(DBG_UDP, "no udp for pkt");
        return NET_ERR_UNREACH;
    }

    pktbuf_remove_header(buf, iphdr_size);
    udp_pkt = (udp_pkt_t *)pktbuf_data(buf);


    if(udp_pkt->hdr.checksum) {
        pktbuf_reset_acc(buf);
        if (checksum_peso(buf, dest, src, NET_PROTOCOL_UDP)) {
            dbg_warning(DBG_UDP, "udp checksum err");
            return NET_ERR_NONE;
        }
    }
    udp_pkt->hdr.src_port = x_ntohs(udp_pkt->hdr.src_port);
    udp_pkt->hdr.dest_port = x_ntohs(udp_pkt->hdr.dest_port);
    udp_pkt->hdr.total_len = x_ntohs(udp_pkt->hdr.total_len);

    if ((err = is_pkt_ok(udp_pkt, buf->total_size)) < 0) {
        dbg_error(DBG_UDP, "udp pkt err");
        return NET_ERR_NONE;
    }

    //recvfrom需要传出数据包原地址，可以像udp一样处理，直接将ip数据包挂到队列，接收函数处理地址信息



    pktbuf_remove_header(buf, sizeof(udp_hdr_t) - sizeof(udp_from_t));
    udp_from_t *from = (udp_from_t *)pktbuf_data(buf);
    ipaddr_copy(&from->from, src);
    from->port = src_port;
    if (nlist_count(&udp->recv_list) <  UDP_MAX_RECV) {
        nlist_insert_last(&udp->recv_list, &buf->node);
        sock_wakeup(&udp->base, SOCK_WAIT_READ, NET_ERR_OK);
        
    } else {
        return NET_ERR_MEM;
    }

    return NET_ERR_OK;
}