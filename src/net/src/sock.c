#include "sock.h"
#include "sys.h"
#include "dbg.h"
#include "net_cfg.h"
#include "raw.h"
#include "socket.h"


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
    nlist_node_init(&sock->node);
    return NET_ERR_OK;

}

net_err_t sock_create_req_in (struct _func_msg_t *msg) {
    //静态查找表，根据不同的套接字类型调用不同的创建函数
    static const struct sock_info_t {
        int protocol;                                   //缺省协议类型
        sock_t * (*create) (int family, int protocol);  //创建回调函数
    } sock_tbl[] = {
        [SOCK_RAW] = {.protocol = IPPROTO_ICMP , .create = raw_create,},
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
    return err;

}