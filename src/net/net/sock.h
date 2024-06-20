#ifndef SOCK_H
#define SOCK_H

#include "net_err.h"
#include "exmsg.h"
#include "ipaddr.h"

typedef int x_socklen_t;
struct _sock_t;
struct x_sockaddr;
struct _sock_req_t;

typedef struct _sock_ops_t{
    net_err_t (*close)(struct _sock_t* s);
	net_err_t (*sendto)(struct _sock_t * s, const void* buf, size_t len, int flags,
                        const struct x_sockaddr* dest, x_socklen_t dest_len, ssize_t * result_len);
	net_err_t(*recvfrom)(struct _sock_t* s, void* buf, size_t len, int flags,
                        struct x_sockaddr* src, x_socklen_t * addr_len, ssize_t * result_len);
	net_err_t (*setopt)(struct _sock_t* s,  int level, int optname, const char * optval, int optlen);
	net_err_t (*connect)(struct _sock_t* s, const struct x_sockaddr* addr, x_socklen_t len);
    void (*destroy)(struct _sock_t *s);
}sock_ops_t;

//sock标识一次具体的通信，保存双方的通信信息，ip，端口，协议，回调函数表
typedef struct _sock_t {
    uint16_t local_port;
    ipaddr_t local_ip;
    uint16_t remote_port;
    ipaddr_t remote_ip;
    const sock_ops_t *ops;

    int family;
    int protocol;

    int err;
    int rcv_tmo;
    int send_tmo;
    nlist_node_t node;


} sock_t;

typedef struct _x_socket_t {
    enum {
        SOCKET_STATE_FREE,
        SOCKET_STATE_USED,
    }state;

    sock_t *sock;

}x_socket_t;

//请求创建socket参数
typedef struct _sock_create_t {
    int family;
    int type;
    int protocol;

}sock_create_t;
//sendto参数结构
typedef struct _sock_data_t {
    uint8_t *buf;
    size_t len;
    int flags;
    struct x_sockaddr *addr;
    x_socklen_t *addr_len;
    ssize_t comp_len;
}sock_data_t;
//通用参数结构
typedef struct _sock_req_t {
    int sockfd;
    union {
        sock_create_t create;
        sock_data_t data;
    };

} sock_req_t;
net_err_t socket_init (void);

net_err_t sock_create_req_in (struct _func_msg_t *msg);
net_err_t sock_sendto_req_in (struct _func_msg_t *msg);
net_err_t sock_recvfrom_req_in (struct _func_msg_t *msg);

//sock初始化
net_err_t sock_init (sock_t *sock, int family, int protocol, const sock_ops_t *ops);


#endif