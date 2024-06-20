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
    net_err_t err = exmsg_func_exec(sock_create_req_in, &req);
    if (err < 0) {
        dbg_error(DBG_SOCKET, "req create socket err");
        return -1;
    }
    return req.sockfd;
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

        net_err_t err = exmsg_func_exec(sock_sendto_req_in, &req);
        if (err < 0) {
            dbg_error(DBG_SOCKET, "req sendto err");
            return -1;
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

    sock_req_t req;
    req.sockfd = s;
    req.data.buf = buf;
    req.data.addr = (struct x_sockaddr *)src;
    req.data.addr_len = src_len;
    req.data.len = len;
    req.data.flags = flags;
    req.data.comp_len = 0;

    net_err_t err = exmsg_func_exec(sock_recvfrom_req_in, &req);
    if (err < 0) {
        dbg_error(DBG_SOCKET, "req sendto err");
        return -1;
    }

    if (req.data.comp_len) {
        return (ssize_t)req.data.comp_len;
    }
    return -1;

}