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
    //查找路由表看是直接交付还是间接交付
    rentry_t *rt = rt_find(&tcp->base.remote_ip);
    if (rt->netif->mtu == 0) {
        tcp->mss = TCP_DEFAULT_MSS;
    } else if (!ipaddr_is_any(&rt->next_hop)) {
        tcp->mss = TCP_DEFAULT_MSS;
    } else {
        tcp->mss = rt->netif->mtu - sizeof(ipv4_hdr_t) - sizeof(tcp_hdr_t);
    }

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



static net_err_t tcp_send (struct _sock_t * s, const void* buf, size_t len, int flags, ssize_t * result_len) {
    tcp_t *tcp = (tcp_t *)s;

    switch (tcp->state)
    {
    //某些状态不能进行数据发送
    case TCP_STATE_CLOSED:
        dbg_error(DBG_TCP, "tcp closed");
        return NET_ERR_CLOSE;

    //主动关闭，不允许发送
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:
    case TCP_STATE_TIME_WAIT:
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_CLOSING:
        dbg_error(DBG_TCP, "tcp closed");
        return NET_ERR_CLOSE;

    //允许发送
    case TCP_STATE_CLOSE_WAIT:
    case TCP_STATE_ESTABLISHED:
        break;


    //未建立连接
    case TCP_STATE_LISTEN:
    case TCP_STATE_SYN_RECVD:
    case TCP_STATE_SYN_SENT:
        dbg_error(DBG_TCP, "tcp state err");
        return NET_ERR_STATE;
    default:
        dbg_error(DBG_TCP, "unknown state");
        return NET_ERR_STATE;
    }

    int size = tcp_write_sendbuf(tcp, (uint8_t *)buf, (int)len);
    if (size <= 0) {
        *result_len = 0;
        return NET_ERR_WAIT;
    } else {
        *result_len = size;
        tcp_transmit(tcp);
        return NET_ERR_OK;
    }
    
}



static net_err_t tcp_recv (struct _sock_t * s, void* buf, size_t len, int flags, ssize_t * result_len) {
    tcp_t *tcp = (tcp_t *)s;
    int wait = NET_ERR_WAIT;

    switch (tcp->state)
    {
    //某些状态不能进行数据接收，lastack主动调用close，关闭接收
    case TCP_STATE_LAST_ACK:
    case TCP_STATE_CLOSED:
    case TCP_STATE_TIME_WAIT:
        dbg_error(DBG_TCP, "tcp closed");
        return NET_ERR_CLOSE;
    //可以读取，没数据不需要等待，因为对方已经发送fin标志位，接收缓冲区已经关闭
    case TCP_STATE_CLOSE_WAIT:
    case TCP_STATE_CLOSING:
        wait = NET_ERR_OK;
        break;

    //允许接收
    case TCP_STATE_ESTABLISHED:
    //主动关闭，可以接收
    case TCP_STATE_FIN_WAIT_1:
    case TCP_STATE_FIN_WAIT_2:

        break;

    //未建立连接
    case TCP_STATE_LISTEN:
    case TCP_STATE_SYN_RECVD:
    case TCP_STATE_SYN_SENT:
        dbg_error(DBG_TCP, "tcp state err");
        return NET_ERR_STATE;
    default:
        dbg_error(DBG_TCP, "unknown state");
        return NET_ERR_STATE;
    }
    *result_len = 0;
    int cnt = tcp_buf_read_recv(&tcp->recv.buf, buf, (int)len);
    if (cnt > 0) {
        *result_len = cnt;
        return NET_ERR_OK;
    }

    return wait; 
}
static net_err_t tcp_setopt (struct _sock_t* s,  int level, int optname, const char * optval, int optlen) {
    net_err_t err = sock_setopt(s, level, optname, optval, optlen);
    if (err == NET_ERR_OK) {
        return NET_ERR_OK;
    } else if (err < 0 && (err != NET_ERR_UNKNOWN)) {
        return err;
    }
    tcp_t *tcp = (tcp_t *)s;
    if (level == SOL_SOCKET) {
        if (optname == SO_KEEPALIVE) {
            if (optlen != sizeof(int)) {
                dbg_error(DBG_TCP, "param err");
                return NET_ERR_PARAM;
            }
            tcp_keepalive_start(tcp, *(int *)optval);
  
            return NET_ERR_OK;
        }
        return NET_ERR_PARAM;
    } else if (level == SOL_TCP) {
        switch (optname)
        {
        case TCP_KEEPIDLE:{
                if (optlen != sizeof(int)) {
                dbg_error(DBG_TCP, "param err");
                return NET_ERR_PARAM;
                }   
            tcp->conn.keep_idle = *(int *)optval;
            tcp_keepalive_restart(tcp);
            break;

        }
        case TCP_KEEPCNT: {
            if (optlen != sizeof(int)) {
                dbg_error(DBG_TCP, "param err");
                return NET_ERR_PARAM;
                }   
            tcp->conn.keep_cnt = *(int *)optval;
            tcp_keepalive_restart(tcp);
            break;
        }
        case TCP_KEEPINTVL: {
            if (optlen != sizeof(int)) {
                dbg_error(DBG_TCP, "param err");
                return NET_ERR_PARAM;
                }   
            tcp->conn.keep_intvl = *(int *)optval;
            tcp_keepalive_restart(tcp);
            break;
        }
        
        default:
            dbg_error(DBG_TCP, "unknown param");
            return NET_ERR_PARAM;
        }
    }

    return NET_ERR_OK;
}
static tcp_t *tcp_alloc (int wait, int family, int protocol) {
        static const sock_ops_t tcp_ops = {
        .connect = tcp_connect,
        .close = tcp_close,
        .setopt = tcp_setopt,
        .send = tcp_send,
        .recv = tcp_recv,
    };
    tcp_t *tcp = tcp_get_free(wait);
    if (!tcp) {
        dbg_error(DBG_TCP, "no tcp sock");
        return (tcp_t *)0;
    }
    plat_memset(tcp, 0, sizeof(tcp_t));
    tcp->state = TCP_STATE_CLOSED;
    tcp->flags.keep_enable = 0;
    tcp->conn.keep_idle = TCP_KEEPALIVE_TIME;
    tcp->conn.keep_intvl = TCP_KEEPINTVL;
    tcp->conn.keep_cnt = TCP_KEEPALIVE_PROBES;
    //静态分配， 可以在前面使用动态分配函数分配一片空间
    tcp_buf_init(&tcp->send.buf, tcp->send.data, TCP_SBUF_SIZE);
    tcp_buf_init(&tcp->recv.buf, tcp->recv.data, TCP_RBUF_SIZE);
    
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
    //超时时间触发已经将定时器移除了，再移除会触发空指针异常
    tcp_kill_all_timers(tcp);
    tcp_set_state(tcp, TCP_STATE_CLOSED);
    //通知所有等待结构
    sock_wakeup(&tcp->base, SOCK_WAIT_ALL, err);
    return NET_ERR_OK;
}


void tcp_read_option (tcp_t *tcp, tcp_hdr_t *hdr) {
    uint8_t *opt_start = (uint8_t *)hdr + sizeof(tcp_hdr_t);
    uint8_t *opt_end = opt_start + (tcp_hdr_size(hdr) - sizeof(tcp_hdr_t));
    while (opt_start < opt_end) {
        switch (opt_start[0])
        {
        case TCP_OPT_MSS:{
            tcp_opt_mss_t *opt = (tcp_opt_mss_t *)opt_start;
            if (opt->length == 4) {
                uint16_t mss = x_ntohs(opt->mss);
                if (mss < tcp->mss) {
                    tcp->mss = mss;
                }
                
            }
            opt_start += opt->length;
            break;
        }
        case TCP_OPT_NOP: {
            opt_start++;
            break;
        }
        case TCP_OPT_END: {
            return;
        }
        default:
            //不一定对，选项字段长度可能不同
            opt_start++;
            break;
        }
    }

}

int tcp_recv_window (tcp_t *tcp) {
    return tcp_buf_free_cnt(&tcp->recv.buf);
}

static void tcp_alive_tmo (struct _net_timer_t *timer, void *arg) {
    tcp_t *tcp = (tcp_t *)arg;
    if (++tcp->conn.keep_retry <= tcp->conn.keep_cnt) {
        tcp_send_keepalive(tcp);
        // net_timer_remove(&tcp->conn.keep_timer);
        net_timer_add(&tcp->conn.keep_timer, "keepalive", tcp_alive_tmo, tcp, tcp->conn.keep_intvl * 1000, 0);
        dbg_info(DBG_TCP, "tcp alive tmo, retry : %d", tcp->conn.keep_retry);
    } else {
        tcp_send_reset_for_tcp(tcp);
        //不能返回tmo，不然recv没法正常退出
        tcp_abort(tcp, NET_ERR_CLOSE);
        dbg_error(DBG_TCP, "alive retry over");

    }
    
}

static void keepalive_start_timer (tcp_t *tcp) {
    net_timer_add(&tcp->conn.keep_timer, "keepalive", tcp_alive_tmo, tcp, tcp->conn.keep_idle * 1000, 0);
    
}

void  tcp_keepalive_start (tcp_t *tcp, int run) {
    if (tcp->flags.keep_enable && !run) {
        net_timer_remove(&tcp->conn.keep_timer);
    } else if (run && !tcp->flags.keep_enable) {
        keepalive_start_timer(tcp);
    }
    tcp->flags.keep_enable = run;
}

void tcp_keepalive_restart (tcp_t *tcp) {
    if (tcp->flags.keep_enable) {
        net_timer_remove(&tcp->conn.keep_timer);
        keepalive_start_timer(tcp);
        tcp->conn.keep_retry = 0;
    }
}

void tcp_kill_all_timers (tcp_t *tcp) {
    net_timer_remove(&tcp->conn.keep_timer);
}
