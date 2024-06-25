#include "socket.h"
#include "exmsg.h"
#include "sock.h"
#include "dbg.h"
#include "net_cfg.h"

int x_socket (int family, int type, int protocol) {
    sock_req_t req;
    req.sockfd = -1;
    req.create.family = family;
    req.create.type = type;
    req.create.protocol = protocol;
    req.wait = (sock_wait_t *)0;
    req.wait_tmo = 0;
    net_err_t err = exmsg_func_exec(sock_create_req_in, &req);
    if (err < 0) {
        dbg_error(DBG_SOCKET, "req create socket err");
        return -1;
    }
    return req.sockfd;
}

int x_bind(int s, const struct x_sockaddr *addr, x_socklen_t len) {
    if ((!addr) || (len != sizeof(struct x_sockaddr)) || (s < 0)) {
        dbg_error(DBG_SOCKET, "param err");
        return -1;
    }
    if (addr->sa_family != AF_INET) {
        dbg_error(DBG_SOCKET, "family err");
        return -1;
    }
    sock_req_t req;
    req.sockfd = s;
    req.bind.addr = addr;
    req.bind.addr_len = len;
    req.wait = (sock_wait_t *)0;
    req.wait_tmo = 0;
    net_err_t err = exmsg_func_exec(sock_bind_req_in, &req);
    if (err < 0) {
        dbg_error(DBG_SOCKET, "req bind err");
        return -1;
    }
    return 0;
}

int x_listen(int s,int backlog) {
    if (backlog <= 0) {
        dbg_error(DBG_SOCKET, "backlog err");
        return NET_ERR_PARAM;
    }
    sock_req_t req;
    req.sockfd = s;
    req.listen.backlog = backlog;
    req.wait = (sock_wait_t *)0;
    req.wait_tmo = 0;
    net_err_t err = exmsg_func_exec(sock_listen_req_in, &req);
    if (err < 0) {
        dbg_error(DBG_SOCKET, "req listen err");
        return -1;
    }
    return 0;
}

int x_accept (int s,  struct x_sockaddr *addr, x_socklen_t *len) {
    if ((!addr) || (*len != sizeof(struct x_sockaddr)) || (s < 0)) {
        dbg_error(DBG_SOCKET, "param err");
        return -1;
    }
    
    sock_req_t req;
    req.sockfd = s;
    req.accept.addr = addr;
    req.accept.len = len;
    req.accept.client = -1;
    req.wait = (sock_wait_t *)0;
    req.wait_tmo = 0;
    net_err_t err = exmsg_func_exec(sock_accept_req_in, &req);
    if (err < 0) {
        dbg_error(DBG_SOCKET, "req accept err");
        return -1;
    }

    if (req.accept.client >= 0) {
        dbg_info(DBG_SOCKET, "new client");
        return req.accept.client;
    }

    if (req.wait) {
        err = sock_wait_enter(req.wait, req.wait_tmo);
        if (err < 0) {
            dbg_error(DBG_SOCKET, "sock wait err");
            return -1;
        }
    }

}

int x_connect(int s, const struct x_sockaddr *addr, x_socklen_t len) {

    if ((!addr) || (len != sizeof(struct x_sockaddr)) || (s < 0)) {
        dbg_error(DBG_SOCKET, "param err");
        return -1;
    }
    if (addr->sa_family != AF_INET) {
        dbg_error(DBG_SOCKET, "family err");
        return -1;
    }
    sock_req_t req;
    req.sockfd = s;
    req.conn.addr = addr;
    req.conn.addr_len = len;
    req.wait = (sock_wait_t *)0;
    req.wait_tmo = 0;
    net_err_t err = exmsg_func_exec(sock_connect_req_in, &req);
    if (err < 0) {
        dbg_error(DBG_SOCKET, "req connect err");
        return -1;
    }
    //tcp连接需要等待，udp不用
    if (req.wait) {
        err = sock_wait_enter(req.wait, req.wait_tmo);
        if (err < 0) {
        dbg_error(DBG_SOCKET, "sock wait err");
        return -1;
        }
    }
    return 0;

}

ssize_t x_sendto(int s, const void *buf, size_t len, int flags, const struct x_sockaddr *dest, x_socklen_t dest_len) {
    if (!buf || !len) {
        dbg_error(DBG_SOCKET, "sento param err");
        return -1;
    }

    if ((dest->sa_family != AF_INET) || (dest_len != sizeof(struct x_sockaddr_in))){
        dbg_error(DBG_SOCKET, "sento param err");
        return -1;
    }

    uint8_t *start = (uint8_t *)buf;
    ssize_t send_size = 0;
    //循环发送
    while (len > 0) {
        sock_req_t req;
        req.sockfd = s;
        req.data.buf = start;
        req.data.addr = (struct x_sockaddr *)dest;
        req.data.addr_len = &dest_len;
        req.data.len = len;
        req.data.flags = flags;
        req.data.comp_len = 0;
        req.wait = (sock_wait_t *)0;
        req.wait_tmo = 0;

        net_err_t err = exmsg_func_exec(sock_sendto_req_in, &req);
        if (err < 0) {
            dbg_error(DBG_SOCKET, "req sendto err");
            return -1;
        }

        if (req.wait) {
            err = sock_wait_enter(req.wait, req.wait_tmo);
            if (err < 0) {
            dbg_error(DBG_SOCKET, "sock wait err");
            return -1;
            }

        }
            
        len -= req.data.comp_len;
        start += req.data.comp_len;
        send_size += (ssize_t)req.data.comp_len;


    }

    return send_size;

}

ssize_t x_send(int s, const void* buf, size_t len, int flags) {
     if (!buf || !len) {
        dbg_error(DBG_SOCKET, "sento param err");
        return -1;
    }


    uint8_t *start = (uint8_t *)buf;
    ssize_t send_size = 0;
    //循环发送
    while (len > 0) {
        sock_req_t req;
        req.sockfd = s;
        req.data.buf = start; 
        req.data.len = len;
        req.data.flags = flags;
        req.data.comp_len = 0;
        req.wait = (sock_wait_t *)0;
        req.wait_tmo = 0;

        net_err_t err = exmsg_func_exec(sock_send_req_in, &req);
        if (err < 0) {
            dbg_error(DBG_SOCKET, "req sendto err");
            return -1;
        }

        if (req.wait) {
            err = sock_wait_enter(req.wait, req.wait_tmo);
            if (err < 0) {
            dbg_error(DBG_SOCKET, "sock wait err");
            return -1;
        }

        }
            
        len -= req.data.comp_len;
        start += req.data.comp_len;
        send_size += (ssize_t)req.data.comp_len;


    }

    return send_size;

}

//不需要将所有数据发完才返回
ssize_t x_recvfrom(int s, void* buf, size_t len, int flags, struct x_sockaddr* src, x_socklen_t* src_len) {
    if (!buf || !len || !src) {
        dbg_error(DBG_SOCKET, "recv param err");
        return -1;
    }
    while (1) {
        sock_req_t req;
        req.sockfd = s;
        req.data.buf = buf;
        req.data.addr = (struct x_sockaddr *)src;
        req.data.addr_len = src_len;
        req.data.len = len;
        req.data.flags = flags;
        req.data.comp_len = 0;
        req.wait = (sock_wait_t *)0;
        req.wait_tmo = 0;

        net_err_t err = exmsg_func_exec(sock_recvfrom_req_in, &req);
        if (err < 0) {
            dbg_error(DBG_SOCKET, "req recvfrom err");
            return -1;
        }

        if (req.data.comp_len) {
            return (ssize_t)req.data.comp_len;
        }

        if (req.wait) {
            err = sock_wait_enter(req.wait, req.wait_tmo);
            if (err == NET_ERR_CLOSE) {
                dbg_info(DBG_SOCKET, "remote closed");
                return 0;
            }
            if (err < 0) {
            dbg_error(DBG_SOCKET, "sock wait err");
            return -1;
            } 

        } else {
                break;
            }

    }
    return -1;
}


ssize_t x_recv(int s, void* buf, size_t len, int flags) {
        if (!buf || !len ) {
        dbg_error(DBG_SOCKET, "recv param err");
        return -1;
    }
    while (1) {
        sock_req_t req;
        req.sockfd = s;
        req.data.buf = buf;

        req.data.len = len;
        req.data.flags = flags;
        req.data.comp_len = 0;
        req.wait = (sock_wait_t *)0;
        req.wait_tmo = 0;

        net_err_t err = exmsg_func_exec(sock_recv_req_in, &req);
        if (err < 0) {
            dbg_error(DBG_SOCKET, "req recv err");
            return -1;
        }

        if (req.data.comp_len) {
            return (ssize_t)req.data.comp_len;
        }

        if (req.wait) {//阻塞等
            err = sock_wait_enter(req.wait, req.wait_tmo);
            if (err == NET_ERR_CLOSE) {
                dbg_info(DBG_SOCKET, "remote closed");
                return 0;
            }
            if (err < 0) {
            dbg_error(DBG_SOCKET, "sock wait err");
            return -1;
            } 

        } else {//非阻塞直接返回
                break;
            }

    }
    return -1;

}

int x_setsockopt(int s, int level, int optname, const char * optval, int len) {

    if (!optval || !len) {
        dbg_error(DBG_SOCKET, "param err");
        return -1;
    }
    sock_req_t req;
    req.sockfd = s;
    req.opt.len = len;
    req.opt.level = level;
    req.opt.optname = optname;
    req.opt.optval = optval;
    req.wait = (sock_wait_t *)0;
    req.wait_tmo = 0;
    net_err_t err = exmsg_func_exec(sock_setsockopt_req_in, &req);
    if (err < 0) {
        dbg_error(DBG_SOCKET, "req  setsocopt err");
        return -1;
    }
    return 0;

}

int x_close(int s) {

    sock_req_t req;
    req.sockfd = s;
    req.wait = (sock_wait_t *)0;
    req.wait_tmo = 0;
    net_err_t err = exmsg_func_exec(sock_close_req_in, &req);
    if (err < 0) {
        dbg_error(DBG_SOCKET, "req close socket err");
        exmsg_func_exec(sock_destory_req_in, &req);
        return -1;
    }


    if (req.wait) {
        //0一直等待
        //err = sock_wait_enter(req.wait, req.wait_tmo);
        err = sock_wait_enter(req.wait, req.wait_tmo);
        if (err < 0) {
            dbg_error(DBG_SOCKET, "sock wait err");
            return -1;
        }
        exmsg_func_exec(sock_destory_req_in, &req);
    } 
    return 0;
}