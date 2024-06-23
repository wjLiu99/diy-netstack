#include "sock.h"
#include "sys.h"
#include "dbg.h"
#include "net_cfg.h"
#include "raw.h"
#include "socket.h"
#include "udp.h"
#include "ipv4.h"
#include "tcp.h"


static x_socket_t socket_tbl[SOCKET_MAX_NR];

//返回socket在表中的索引
static int get_index (x_socket_t *s) {
    return (int)(s - socket_tbl);
}

static x_socket_t *get_socket (int idx) {
    if ((idx < 0) || (idx >= SOCKET_MAX_NR)) {
        return (x_socket_t *)0;
    }

    return socket_tbl + idx;
}

//socket分配，直接查表不用mblock
static x_socket_t *socket_alloc (void) {
    x_socket_t *s = (x_socket_t *)0;
    for (int i = 0; i < SOCKET_MAX_NR; i++) {
        x_socket_t *cur = socket_tbl + i;
        if (cur->state == SOCKET_STATE_FREE) {
            cur->state = SOCKET_STATE_USED;
            s = cur;
            break;
        }
    }

    return s;
}
//socket释放
static void socket_free (x_socket_t *s) {
    s->state = SOCKET_STATE_FREE;

}


net_err_t socket_init (void) {
    plat_memset(socket_tbl, 0, sizeof(socket_tbl));

    return NET_ERR_OK;
}

net_err_t sock_wait_init (sock_wait_t *wait) {
    wait->waiting = 0;
    wait->err = NET_ERR_OK;
    wait->sem = sys_sem_create(0);
    return wait->sem == SYS_SEM_INVALID ? NET_ERR_SYS : NET_ERR_OK;
}

void sock_wait_destory (sock_wait_t *wait) {
    if (wait->sem != SYS_SEM_INVALID) {
        sys_sem_free(wait->sem);
    }
}

void sock_wait_add (sock_wait_t *wait, int tmo, struct _sock_req_t *req) {
    //wait等待结构上等待程序数量增加， 初始化请求中的等待结构让应用程序在该等待结构上等待
    wait->waiting++;
    req->wait = wait;
    req->wait_tmo = tmo;

}   
net_err_t sock_wait_enter (sock_wait_t *wait, int tmo) {
    //应用程序调用， 如果信号量被初始化了就在该信号量上等待,初始化tmo为0是一直阻塞等待，没有超时
    if (sys_sem_wait(wait->sem, tmo) < 0) {
        return NET_ERR_TMO;
    }
    return wait->err;
}                

void sock_wait_leave (sock_wait_t *wait, net_err_t err) {
    //数据到达， 工作线程调用， 通知应用程序，等待程序减一
    if (wait->waiting > 0) {
        wait->waiting--;
        sys_sem_notify(wait->sem);
        wait->err = err;
    }
} 

//有数据到达时调用
void sock_wakeup (sock_t *sock, int type, int err) {
    if (type & SOCK_WAIT_CONN) {
        sock_wait_leave(sock->conn_wait, err);
    }
    if (type & SOCK_WAIT_READ) {
        sock_wait_leave(sock->recv_wait, err);
    }
    if (type & SOCK_WAIT_WRITE) {
        sock_wait_leave(sock->send_wait, err);
    }
}

net_err_t sock_init (sock_t *sock, int family, int protocol, const sock_ops_t *ops) {
    sock->family = family;
    sock->ops = ops;
    sock->protocol = protocol;

    ipaddr_set_any(&sock->local_ip);
    ipaddr_set_any(&sock->remote_ip);
    sock->remote_port = 0;
    sock->local_port = 0;
    sock->err = NET_ERR_OK;
    sock->rcv_tmo = 0;
    sock->send_tmo = 0;
    sock->conn_wait = (sock_wait_t *)0;
    sock->send_wait = (sock_wait_t *)0;
    sock->recv_wait = (sock_wait_t *)0;
    nlist_node_init(&sock->node);
    return NET_ERR_OK;

}

net_err_t sock_uninit (sock_t *sock) {
    if (sock->recv_wait) {
        sock_wait_destory(sock->recv_wait);
    }
    if (sock->send_wait) {
    sock_wait_destory(sock->send_wait);
    }
    if (sock->conn_wait) {
    sock_wait_destory(sock->conn_wait);
    }

    return NET_ERR_OK;
}

net_err_t sock_setopt (struct _sock_t* s,  int level, int optname, const char * optval, int optlen) {
    if (level != SOL_SOCKET) {
        dbg_error(DBG_SOCKET, "unknown level");
        return NET_ERR_PARAM;
    }

    switch (optname)
    {
    case SO_RCVTIMEO:

    case SO_SNDTIMEO: {
        if (optlen != sizeof(struct x_timeval)) {
            dbg_error(DBG_SOCKET, "timeval err");
            return NET_ERR_PARAM;
        }
        struct x_timeval *time = (struct x_timeval *)optval;
        int time_ms = time->tv_sec * 1000 + time->tv_usec / 1000;
        if (optname == SO_RCVTIMEO) {
            s->rcv_tmo = time_ms;
            return NET_ERR_OK;
        } else if (optname == SO_SNDTIMEO) {
            s->send_tmo = time_ms;
            return NET_ERR_OK;
        } else {
            return NET_ERR_PARAM;
        }

        break;
    }
    
    default:
        break;
    }

    return NET_ERR_PARAM;
}

net_err_t sock_create_req_in (struct _func_msg_t *msg) {
    //静态查找表，根据不同的套接字类型调用不同的创建函数
    static const struct sock_info_t {
        int protocol;                                   //缺省协议类型
        sock_t * (*create) (int family, int protocol);  //创建回调函数
    } sock_tbl[] = {
        [SOCK_RAW] = {.protocol = IPPROTO_ICMP , .create = raw_create,},
        [SOCK_DGRAM] = {.protocol = IPPROTO_UDP, .create = udp_create},
        [SOCK_STREAM] = {.protocol = IPPROTO_TCP, .create = tcp_create},
    };
    sock_req_t *req = (sock_req_t *)msg->param;
    sock_create_t *param = &req->create;
    x_socket_t *s = socket_alloc();
    if (!s) {
        dbg_error(DBG_SOCKET, "no socket");
        return NET_ERR_MEM;
    }

    if ((param->type  < 0) || (param->type >= sizeof(sock_tbl) / sizeof(sock_tbl[0]))) {
        dbg_error(DBG_SOCKET, "sock type err");
        socket_free(s);
        return NET_ERR_PARAM;
    }
    const struct sock_info_t *info = sock_tbl + param->type;
    sock_t *sock = info->create(param->family, param->protocol ? param->protocol : info->protocol);
    if (!sock) {
        dbg_error(DBG_SOCKET, "create sock failed");
        socket_free(s);
        return NET_ERR_MEM;
    }

    s->sock = sock;

    req->sockfd = get_index(s);
    return NET_ERR_OK;
}


net_err_t sock_bind_req_in (struct _func_msg_t *msg) {
    sock_req_t *req = (sock_req_t *)msg->param;
    
    x_socket_t *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param err");
        return NET_ERR_PARAM;
    }
    sock_t *sock = s->sock;
    sock_bind_t *bind = &req->bind;

    if (!sock->ops->bind) {
        dbg_error(DBG_SOCKET, "func not exist");
        return NET_ERR_EXIST;
    }

    return  sock->ops->bind(sock, bind->addr, bind->addr_len);
    //bind不需要等待

}


net_err_t sock_connect_req_in (struct _func_msg_t *msg) {
    sock_req_t *req = (sock_req_t *)msg->param;
    
    x_socket_t *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param err");
        return NET_ERR_PARAM;
    }
    sock_t *sock = s->sock;
    sock_conn_t *conn = &req->conn;

    if (!sock->ops->connect) {
        dbg_error(DBG_SOCKET, "func not exist");
        return NET_ERR_EXIST;
    }

    net_err_t err = sock->ops->connect(sock, conn->addr, conn->addr_len);
    if (err == NET_ERR_WAIT) {
        if (sock->send_wait) {
            sock_wait_add(sock->conn_wait, sock->rcv_tmo, req);
        }

    }

    return err;

}

net_err_t sock_sendto_req_in (struct _func_msg_t *msg) {
    sock_req_t *req = (sock_req_t *)msg->param;
    
    x_socket_t *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param err");
        return NET_ERR_PARAM;
    }
    sock_t *sock = s->sock;
    sock_data_t *data = &req->data;

    if (!sock->ops->sendto) {
        dbg_error(DBG_SOCKET, "func not exist");
        return NET_ERR_EXIST;
    }

    net_err_t err = sock->ops->sendto(sock, data->buf, data->len, data->flags, data->addr, *data->addr_len, &data->comp_len);
    if (err == NET_ERR_WAIT) {
        if (sock->send_wait) {
            sock_wait_add(sock->send_wait, sock->send_tmo, req);
        }

    }

    return err;

}

net_err_t sock_send_req_in (struct _func_msg_t *msg) {
    sock_req_t *req = (sock_req_t *)msg->param;
    
    x_socket_t *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param err");
        return NET_ERR_PARAM;
    }
    sock_t *sock = s->sock;
    sock_data_t *data = &req->data;

    if (!sock->ops->send) {
        dbg_error(DBG_SOCKET, "func not exist");
        return NET_ERR_EXIST;
    }

    net_err_t err = sock->ops->send(sock, data->buf, data->len, data->flags,  &data->comp_len);
    if (err == NET_ERR_WAIT) {
        if (sock->send_wait) {
            sock_wait_add(sock->send_wait, sock->send_tmo, req);
        }

    }

    return err;

}


net_err_t sock_recvfrom_req_in (struct _func_msg_t *msg) {

    sock_req_t *req = (sock_req_t *)msg->param;
    
    x_socket_t *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param err");
        return NET_ERR_PARAM;
    }
    sock_t *sock = s->sock;
    sock_data_t *data = &req->data;

    if (!sock->ops->recvfrom) {
        dbg_error(DBG_SOCKET, "func not exist");
        return NET_ERR_EXIST;
    }

    net_err_t err = sock->ops->recvfrom(sock, data->buf, data->len, data->flags, data->addr, data->addr_len, &data->comp_len);
    
    if (err == NET_ERR_WAIT) {
        if (sock->recv_wait) {
            sock_wait_add(sock->recv_wait, sock->rcv_tmo, req);
        }

    }
    return err;

}


net_err_t sock_recv_req_in (struct _func_msg_t *msg) {

    sock_req_t *req = (sock_req_t *)msg->param;
    
    x_socket_t *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param err");
        return NET_ERR_PARAM;
    }
    sock_t *sock = s->sock;
    sock_data_t *data = &req->data;

    if (!sock->ops->recv) {
        dbg_error(DBG_SOCKET, "func not exist");
        return NET_ERR_EXIST;
    }

    net_err_t err = sock->ops->recv(sock, data->buf, data->len, data->flags, &data->comp_len);
    
    if (err == NET_ERR_WAIT) {
        if (sock->recv_wait) {
            sock_wait_add(sock->recv_wait, sock->rcv_tmo, req);
        }

    }
    return err;

}

net_err_t sock_setsockopt_req_in (struct _func_msg_t *msg) {
    sock_req_t *req = (sock_req_t *)msg->param;
    
    x_socket_t *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param err");
        return NET_ERR_PARAM;
    }
    sock_t *sock = s->sock;
    sock_opt_t *opt = (sock_opt_t *)&req->opt;
    if (!sock->ops->setopt) {
        dbg_error(DBG_SOCKET, "func not exist");
        return NET_ERR_EXIST;
    }

    return sock->ops->setopt(sock, opt->level, opt->optname, opt->optval, opt->len);

}

net_err_t sock_close_req_in (struct _func_msg_t *msg) {
    sock_req_t *req = (sock_req_t *)msg->param;
    
    x_socket_t *s = get_socket(req->sockfd);
    if (!s) {
        dbg_error(DBG_SOCKET, "param err");
        return NET_ERR_PARAM;
    }
    sock_t *sock = s->sock;
    sock_opt_t *opt = (sock_opt_t *)&req->opt;
    if (!sock->ops->close) {
        dbg_error(DBG_SOCKET, "func not exist");
        return NET_ERR_EXIST;
    }
    socket_free(s);
    net_err_t err = sock->ops->close(sock);
    
    return err;
}

net_err_t sock_connect (struct _sock_t* s, const struct x_sockaddr* addr, x_socklen_t len) {
    struct x_sockaddr_in *remote = (struct x_sockaddr_in *)addr;
    ipaddr_from_buf(&s->remote_ip, remote->sin_addr.addr_array);
    s->remote_port = x_ntohs(remote->sin_port);
    return NET_ERR_OK;

}

net_err_t sock_bind (struct _sock_t* s, const struct x_sockaddr* addr, x_socklen_t len) {
    ipaddr_t local_ip;
    struct x_sockaddr_in *local = (struct x_sockaddr_in *)addr;
    ipaddr_from_buf(&local_ip, local->sin_addr.addr_array);
    //查看路由表，看是否是本地真实的ip地址
    if (!ipaddr_is_any(&local_ip)) {
        rentry_t *rt = rt_find(&local_ip);
        if (!rt || (!ipaddr_is_equal(&rt->netif->ipaddr, &local_ip))) {
            dbg_error(DBG_SOCKET, "addr err");
            return NET_ERR_PARAM;
        }
    }
    ipaddr_copy(&s->local_ip, &local_ip);
    s->local_port = x_ntohs(local->sin_port);
    return NET_ERR_OK;
}

net_err_t sock_send (struct _sock_t * s, const void* buf, size_t len, int flags, ssize_t * result_len) {
    struct x_sockaddr_in dest;
    dest.sin_family = s->family;
    dest.sin_port = x_htons(s->remote_port);
    ipaddr_to_buf(&s->remote_ip, dest.sin_addr.addr_array);
    return s->ops->sendto(s, buf, len, flags, (const struct x_sockaddr *)&dest, sizeof(dest), result_len);

}

net_err_t sock_recv (struct _sock_t * s, void* buf, size_t len, int flags, ssize_t * result_len) {
    //没有使用的地址结构
    struct x_sockaddr src;
    x_socklen_t addr_len;
    return s->ops->recvfrom(s, buf, len, flags, &src, &addr_len, result_len);
}