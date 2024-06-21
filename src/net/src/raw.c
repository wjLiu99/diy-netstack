#include "raw.h"
#include "dbg.h"
#include "mblock.h"
#include "nlist.h"
#include "net_cfg.h"
#include "ipv4.h"
#include "socket.h"
#include "sock.h"

static raw_t raw_tbl[RAW_MAX_NR];
static mblock_t raw_mblock;
static nlist_t raw_list;

#if DBG_DISPLAY_ENABLED(DBG_RAW)
static void display_raw_list (void) {
    plat_printf("\n--- raw list\n --- ");

    int idx = 0;
    nlist_node_t * node;

    nlist_for_each(node, &raw_list) {
        raw_t * raw = (raw_t *)nlist_entry(node, sock_t, node);
        plat_printf("[%d]\n", idx++);
        dbg_dump_ip_buf("\tlocal:", (const uint8_t *)&raw->base.local_ip.a_addr);
        dbg_dump_ip_buf("\tremote:", (const uint8_t *)&raw->base.remote_ip.a_addr);
    }

    plat_printf("\n--- raw end\n --- ");
}
#else
#define display_raw_list()
#endif


net_err_t raw_init(void) {
    dbg_info(DBG_RAW, "raw init..");
    mblock_init(&raw_mblock, raw_tbl, sizeof(raw_t), RAW_MAX_NR, NLOCKER_NONE);
    nlist_init(&raw_list);
    dbg_info(DBG_RAW, "raw init done");
    return NET_ERR_OK;

}

static net_err_t raw_sendto (struct _sock_t *s, const void *buf, size_t len, int flags, 
    const struct x_sockaddr *dest, x_socklen_t dest_len, ssize_t *result_len ) {
    
  

    ipaddr_t dest_ip;
    struct x_sockaddr_in *addr = (struct x_sockaddr_in *)dest;
    ipaddr_from_buf(&dest_ip, addr->sin_addr.addr_array);
    //如果sock绑定了地址，发送地址必须相同
    if (!ipaddr_is_any(&s->remote_ip) && !ipaddr_is_equal(&dest_ip, &s->remote_ip)) {
        dbg_error(DBG_RAW, "dest is incorrect");
        return NET_ERR_PARAM;
    }

    pktbuf_t *pktbuf = pktbuf_alloc((int)len);
    if (!pktbuf) {
        dbg_error(DBG_RAW, "no pktbuf");
        return NET_ERR_MEM;
    }
    pktbuf_reset_acc(pktbuf);
    net_err_t err = pktbuf_write(pktbuf, (uint8_t *)buf, (int)len);
    if (err < 0) {
        dbg_error(DBG_RAW, "copy_data err");
        goto end;
    }

    err = ipv4_out(s->protocol, &dest_ip, &netif_get_default()->ipaddr, pktbuf);
    if (err < 0) {
        dbg_error(DBG_RAW, "send err");
        goto end;
    }
    *result_len = (ssize_t)len;
    return NET_ERR_OK;
end:
    pktbuf_free(pktbuf);
    return err;
}


static net_err_t raw_recvfrom (struct _sock_t *s,  void *buf, size_t len, int flags, 
     struct x_sockaddr *src, x_socklen_t *src_len, ssize_t *result_len ){

    raw_t *raw = (raw_t *)s;
    nlist_node_t *first = nlist_remove_first(&raw->recv_list); 
    //输入队列中没有数据则返回需要等待
    if (!first) {
        *result_len = 0;
        return NET_ERR_WAIT;
    }
    
    //有数据则开始读
    pktbuf_t *pktbuf = (pktbuf_t *)nlist_entry(first, pktbuf_t, node);
    ipv4_hdr_t *ipdhr = (ipv4_hdr_t *)pktbuf_data(pktbuf);

    struct x_sockaddr_in *addr = (struct x_sockaddr_in *)src;
    plat_memset(addr, 0, sizeof(struct x_sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = 0;
    plat_memcpy(&addr->sin_addr, ipdhr->src_ip, IPV4_ADDR_SIZE);

    int size = (pktbuf->total_size >(int)len) ? (int)len : pktbuf->total_size;
    //将ip包头一起读出
    pktbuf_reset_acc(pktbuf);
    net_err_t err = pktbuf_read(pktbuf, buf, size);
    if (err < 0) {
        dbg_error(DBG_RAW, "raw pkt read err");
        //把pktbuf插入输入队列已经返回正确了，这里是pktbuf最后一个阶段，只用读入用户缓冲区，全部交由该函数释放
        pktbuf_free(pktbuf);
        return err;
    }
    pktbuf_free(pktbuf);

    //返回实际读出字节数
    *result_len = size;
    return NET_ERR_OK;

}

net_err_t raw_close (sock_t *sock) {
    raw_t *raw = (raw_t *)sock;
    nlist_remove(&raw_list, &sock->node);

    nlist_node_t *node;
    while ((node = nlist_remove_first(&raw->recv_list))) {
        pktbuf_t *buf = nlist_entry(node, pktbuf_t, node);
        pktbuf_free(buf);
    }
    sock_uninit(sock);
    mblock_free(&raw_mblock, raw);
    display_raw_list();
    return NET_ERR_OK;
}

sock_t *raw_create (int family, int protocol) {

    static const sock_ops_t raw_ops = {
        .sendto = raw_sendto,
        .recvfrom = raw_recvfrom,
        .setopt = sock_setopt,
        .close = raw_close,

    };
    raw_t *raw = mblock_alloc(&raw_mblock, -1);
    if (!raw) {
        dbg_error(DBG_RAW, "raw alloc err");
        return (sock_t *)0;
    }

    net_err_t err = sock_init(&raw->base, family, protocol, &raw_ops);
    if (err < 0) {
        dbg_error(DBG_RAW, "create raw failed");
        mblock_free(&raw_mblock, raw);
        return (sock_t *)0;
    }

    nlist_init(&raw->recv_list);
    raw->base.recv_wait = &raw->recv_wait;
    
    if (sock_wait_init(raw->base.recv_wait) < 0) {
        dbg_error(DBG_RAW, "init recv wait err");
        goto create_failed;
    }


    nlist_insert_last(&raw_list, &raw->base.node);
    display_raw_list();

    return (sock_t *)raw;

create_failed:
    sock_uninit(&raw->base);
    return (sock_t *)0;
}

//因为是对方发来的数据包，src对端地址，dest本机地址
static raw_t * raw_find(ipaddr_t *src, ipaddr_t *dest, int protocol) {
    nlist_node_t *node;
    nlist_for_each(node, &raw_list) {
        raw_t *raw = (raw_t *)nlist_entry(node, sock_t, node);
        //协议为0就表示为空，会选择一个缺省协议
        if (raw->base.protocol && (raw->base.protocol != protocol)) {
            continue;
        }
        //如果套接字没有绑定地址，远程地址可能为空，不为空才比较
        if (!ipaddr_is_any(&raw->base.remote_ip) && ipaddr_is_equal(&raw->base.remote_ip, src)) {
            continue;
        }

        if (!ipaddr_is_any(&raw->base.local_ip) && !ipaddr_is_equal(&raw->base.local_ip, dest)) {
            continue;
        }

        return raw;
    }

    return (raw_t *)0;
}
//输入完整的ip数据包，包含包头
net_err_t raw_in (pktbuf_t *buf) {
    ipv4_hdr_t *iphdr = (ipv4_hdr_t *)pktbuf_data(buf);

    ipaddr_t src, dest;
    ipaddr_from_buf(&dest, iphdr->dest_ip);
    ipaddr_from_buf(&src, iphdr->src_ip);

    raw_t *raw = raw_find(&src, &dest, iphdr->protocol);
    if (!raw) {
        dbg_warning(DBG_RAW, "no raw find");
        return NET_ERR_UNREACH;
    }

    if (nlist_count(&raw->recv_list) <  RAW_MAX_RECV) {
        nlist_insert_last(&raw->recv_list, &buf->node);
        sock_wakeup(&raw->base, SOCK_WAIT_READ, NET_ERR_OK);
        
    } else {
        return NET_ERR_MEM;
    }

    return NET_ERR_OK;
}