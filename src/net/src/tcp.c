#include "tcp.h"
#include "dbg.h"
#include "mblock.h"
#include "protocol.h"
#include "sock.h"
#include "ipaddr.h"
#include "net_api.h"
#include "ipv4.h"
#include "tcp_out.h"
#include "tcp_stat.h"

static tcp_t tcp_tbl[TCP_MAX_NR];
static mblock_t tcp_mblock;
static nlist_t tcp_list;

#if DBG_DISPLAY_ENABLED(DBG_TCP)

void tcp_show_info (char * msg, tcp_t * tcp) {
    plat_printf("%s\n", msg);
    plat_printf("    local port: %u, remote port: %u\n", tcp->base.local_port, tcp->base.remote_port);
}

void tcp_display_pkt (char * msg, tcp_hdr_t * tcp_hdr, pktbuf_t * buf) {
    plat_printf("%s\n", msg);
    plat_printf("    sport: %u, dport: %u\n", tcp_hdr->sport, tcp_hdr->dport);
    plat_printf("    seq: %u, ack: %u, win: %d\n", tcp_hdr->seq, tcp_hdr->ack, tcp_hdr->win);
    plat_printf("    flags:");
    if (tcp_hdr->f_syn) {
        plat_printf(" syn");
    }
    if (tcp_hdr->f_rst) {
        plat_printf(" rst");
    }
    if (tcp_hdr->f_ack) {
        plat_printf(" ack");
    }
    if (tcp_hdr->f_psh) {
        plat_printf(" push");
    }
    if (tcp_hdr->f_fin) {
        plat_printf(" fin");
    }

    plat_printf("\n    len=%d", buf->total_size - tcp_hdr_size(tcp_hdr));
    plat_printf("\n");
}

void tcp_show_list (void) {
    char idbuf[10];
    int i = 0;

    plat_printf("-------- tcp list -----\n");

    nlist_node_t * node;
    nlist_for_each(node, &tcp_list) {
        tcp_t * tcp = (tcp_t *)nlist_entry(node, sock_t, node);

        plat_memset(idbuf, 0, sizeof(idbuf));
        plat_printf(idbuf, "%d:", i++);
        tcp_show_info(idbuf, tcp);
    }
}

#endif


net_err_t tcp_init (void) {
    dbg_info(DBG_TCP, "tcp init");
    mblock_init(&tcp_mblock, tcp_tbl, sizeof(tcp_t), TCP_MAX_NR, NLOCKER_NONE);
    nlist_init(&tcp_list);
    dbg_info(DBG_TCP, "tcp init done");
    return NET_ERR_OK;
}
static int tcp_alloc_port (void) {
    static int search_idx = NET_PORT_DYN_START;
    
    for (int i = NET_PORT_DYN_START; i < NET_PORT_DYN_END; i++) {
        nlist_node_t *node;
        nlist_for_each(node, &tcp_list) {
            sock_t *s = nlist_entry(node, sock_t, node);
            if (s->local_port == search_idx) {
                break;
            }
        }
        if (!node) {
            return search_idx;
        }
        if (++search_idx > NET_PORT_DYN_END) {
            search_idx = NET_PORT_DYN_START;
        }
        
    }
    return -1;
}
//简单实现，可以换别的算法
static uint32_t tcp_get_iss (void) {
    static uint32_t seq = 1;
    return seq++;
}
static net_err_t tcp_init_connect(tcp_t *tcp){
    tcp->send.iss = tcp_get_iss();
    tcp->send.una = tcp->send.nxt = tcp->send.iss;

    tcp->recv.nxt = 0;
    return NET_ERR_OK;

}
net_err_t tcp_connect (struct _sock_t* s, const struct x_sockaddr* addr, x_socklen_t len) {
    tcp_t *tcp = (tcp_t *)s;
    if (tcp->state != TCP_STATE_CLOSED) {
        dbg_error(DBG_TCP, "tcp is not closed");
        return NET_ERR_STATE;
    }
    const struct x_sockaddr_in *addr_in = (const struct x_sockaddr_in *)addr;
    ipaddr_from_buf(&s->remote_ip, (uint8_t *)&addr_in->sin_addr.s_addr);
    //大端转小端保存
    s->remote_port = x_ntohs(addr_in->sin_port);
    //没有绑定端口就分配一个端口
    if (s->local_port == NET_PORT_EMPTY) {
        int port = tcp_alloc_port();
        if (port == -1) {
            dbg_error(DBG_TCP, "alloc port err");
            return NET_ERR_NONE;
        }

        s->local_port =port;
    }
    //可以不查ip地址，因为网卡可能手动禁用就不能再用这个地址通信了，客户端应该只用指定远端的地址和端口就能通信
    if (ipaddr_is_any(&s->local_ip)) {
        rentry_t *rt = rt_find(&s->remote_ip);
        if (rt == (rentry_t *)0) {
            dbg_error(DBG_TCP, "no route host");
            return NET_ERR_UNREACH;
        }
        ipaddr_copy(&s->local_ip, &rt->netif->ipaddr);
    }

    net_err_t err;
    if ((err = tcp_init_connect(tcp)) < 0) {
        dbg_error(DBG_TCP, "init conn failed");
        return err;
    }

    if ((err = tcp_send_syn(tcp)) < 0) {
        dbg_error(DBG_TCP, "send syn err");
        return err;
    }
    tcp_set_state(tcp, TCP_STATE_SYN_SENT);
    return NET_ERR_WAIT;
}

//tcp释放
void tcp_free (tcp_t *tcp) {
    sock_wait_destory(&tcp->conn.wait);
    sock_wait_destory(&tcp->recv.wait);
    sock_wait_destory(&tcp->send.wait);
    tcp->state = TCP_STATE_FREE;
    nlist_remove(&tcp_list, &tcp->base.node);
    mblock_free(&tcp_mblock, tcp);

}

net_err_t tcp_close (sock_t *sock) {
    tcp_t *tcp = (tcp_t *)sock;
    switch (tcp->state)
    {
    case TCP_STATE_CLOSED:
        dbg_error(DBG_TCP, "tcp already closed");
        tcp_free(tcp);
        return NET_ERR_OK;
    
    case TCP_STATE_SYN_SENT:
    case TCP_STATE_SYN_RECVD:
        tcp_abort(tcp, NET_ERR_CLOSE);
        tcp_free(tcp);
        return NET_ERR_OK;


    case TCP_STATE_CLOSE_WAIT:
        tcp_send_fin(tcp);
        tcp_set_state(tcp, TCP_STATE_LAST_ACK);
        //需要等对方ack，不能直接返回，调用close后要等对方ack
        return NET_ERR_WAIT;
    case TCP_STATE_ESTABLISHED:
        //连接状态调用close，主动发送fin报文，切换状态等待对方响应
        tcp_send_fin(tcp);
        tcp_set_state(tcp, TCP_STATE_FIN_WAIT_1);

        return NET_ERR_WAIT;
    
    default:
        dbg_error(DBG_TCP, "tcp state err");
        return NET_ERR_STATE;
    
    }
    return NET_ERR_OK;
}



static tcp_t *tcp_get_free (int wait) {
    tcp_t *tcp = mblock_alloc(&tcp_mblock, wait ? 0 : -1);
    if (!tcp) {
        dbg_error(DBG_TCP, "tcp alloc err");
        return (tcp_t *)0;
    }
    return tcp;
}
static tcp_t *tcp_alloc (int wait, int family, int protocol) {
        static const sock_ops_t tcp_ops = {
        .connect = tcp_connect,
        .close = tcp_close,
        .setopt = sock_setopt,
        .send = sock_send,
        .recv = sock_recv,
    };
    tcp_t *tcp = tcp_get_free(wait);
    if (!tcp) {
        dbg_error(DBG_TCP, "no tcp sock");
        return (tcp_t *)0;
    }
    plat_memset(tcp, 0, sizeof(tcp_t));
    tcp->state = TCP_STATE_CLOSED;
    
    net_err_t err = sock_init(&tcp->base, family, protocol, &tcp_ops);
    if (err < 0) {
        dbg_error(DBG_TCP, "tcp sock init err");
        mblock_free(&tcp_mblock, tcp);
        return (tcp_t *)0;
    }
    if (sock_wait_init(&tcp->conn.wait) < 0) {
        dbg_error(DBG_TCP, "init conn wait err");
        goto alloc_failed;
    }
    //设置了sock等待结构才能在req的时候用sock添加等待
    tcp->base.conn_wait = &tcp->conn.wait;

    if (sock_wait_init(&tcp->send.wait) < 0) {
        dbg_error(DBG_TCP, "init conn wait err");
        goto alloc_failed;
    }
    //设置了sock等待结构才能在req的时候用sock添加等待
    tcp->base.send_wait = &tcp->send.wait;

    if (sock_wait_init(&tcp->recv.wait) < 0) {
        dbg_error(DBG_TCP, "init conn wait err");
        goto alloc_failed;
    }
    //设置了sock等待结构才能在req的时候用sock添加等待
    tcp->base.recv_wait = &tcp->recv.wait;

    return tcp;
alloc_failed:
    if(tcp->base.conn_wait) {
        sock_wait_destory(tcp->base.conn_wait);
    }
    if(tcp->base.recv_wait) {
        sock_wait_destory(tcp->base.recv_wait);
    }
    if(tcp->base.send_wait) {
        sock_wait_destory(tcp->base.send_wait);
    }
    mblock_free(&tcp_mblock, tcp);
    return (tcp_t *)0;
}

static void tcp_insert (tcp_t *tcp) {
    nlist_insert_last(&tcp_list, &tcp->base.node);
    // dbg_assert(tcp_list.count <= TCP_MAX_NR, "tcp count err");
}
sock_t *tcp_create (int family, int protocol) {
    tcp_t *tcp = tcp_alloc(1, family, protocol);
    if (!tcp) {
        dbg_error(DBG_TCP, "alloc tcp failed");
        return (sock_t *)0;
    }
    tcp_insert(tcp);
    return (sock_t *)tcp;
}

tcp_t * tcp_find (ipaddr_t *local_ip, uint16_t local_port, ipaddr_t *remote_ip, uint16_t remote_port) {
    tcp_t *tcp = (tcp_t *)0;
    nlist_node_t *node;
    nlist_for_each(node, &tcp_list) {
        sock_t *s = nlist_entry(node, sock_t, node);
        if (!ipaddr_is_any(&s->local_ip) && !ipaddr_is_equal(&s->local_ip, local_ip)) {
            continue;
        }
        if (
            (s->local_port == local_port) &&
            (ipaddr_is_equal(&s->remote_ip, remote_ip)) &&
            (s->remote_port == remote_port)
        ) {
            tcp = (tcp_t *)s;
            break;
        }
    }

    return tcp;
}

net_err_t tcp_abort(tcp_t *tcp, net_err_t err) {
    tcp_set_state(tcp, TCP_STATE_CLOSED);
    //通知所有等待结构
    sock_wakeup(&tcp->base, SOCK_WAIT_ALL, err);
    return NET_ERR_OK;
}