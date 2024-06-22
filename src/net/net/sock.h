#ifndef SOCK_H
#define SOCK_H

#include "net_err.h"
#include "exmsg.h"
#include "ipaddr.h"

typedef int x_socklen_t;
struct _sock_t;
struct x_sockaddr;
struct _sock_req_t;

#define SOCK_WAIT_READ      (1 << 0)
#define SOCK_WAIT_WRITE     (1 << 1)
#define SOCK_WAIT_CONN      (1 << 2)
#define SOCK_WAIT_ALL       (SOCK_WAIT_READ | SOCK_WAIT_WRITE | SOCK_WAIT_ALL)

//等待结构
typedef struct _sock_wait_t {
    sys_sem_t sem;
    net_err_t err;
    int waiting;
}sock_wait_t;
net_err_t sock_wait_init (sock_wait_t *wait);
void sock_wait_destory (sock_wait_t *wait);
void sock_wait_add (sock_wait_t *wait, int tmo, struct _sock_req_t *req);   //由工作线程调用
net_err_t sock_wait_enter (sock_wait_t *wait, int tmo);                     //应用程序等待

void sock_wait_leave (sock_wait_t *wait, net_err_t err);                    //通知应用程序退出等待结构，以及什么原因退出

typedef struct _sock_ops_t{
    net_err_t (*close)(struct _sock_t* s);
	net_err_t (*sendto)(struct _sock_t * s, const void* buf, size_t len, int flags,
                        const struct x_sockaddr* dest, x_socklen_t dest_len, ssize_t * result_len);
    net_err_t (*send)(struct _sock_t * s, const void* buf, size_t len, int flags, ssize_t * result_len);
	net_err_t(*recvfrom)(struct _sock_t* s, void* buf, size_t len, int flags,
                        struct x_sockaddr* src, x_socklen_t * addr_len, ssize_t * result_len);
    net_err_t(*recv)(struct _sock_t* s, void* buf, size_t len, int flags, ssize_t * result_len);
	net_err_t (*setopt)(struct _sock_t* s,  int level, int optname, const char * optval, int optlen);
	net_err_t (*connect)(struct _sock_t* s, const struct x_sockaddr* addr, x_socklen_t len);
    net_err_t (*bind)(struct _sock_t* s, const struct x_sockaddr* addr, x_socklen_t len);
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

    //指针， 有些sock不需要
    sock_wait_t *recv_wait;
    sock_wait_t *send_wait;
    sock_wait_t *conn_wait;
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

typedef struct _sock_conn_t {
    const struct x_sockaddr *addr;
    x_socklen_t addr_len;
    
}sock_conn_t;

typedef struct _sock_bind_t {
    const struct x_sockaddr *addr;
    x_socklen_t addr_len;
    
}sock_bind_t;

//sendto参数结构
typedef struct _sock_data_t {
    uint8_t *buf;
    size_t len;
    int flags;
    struct x_sockaddr *addr;
    x_socklen_t *addr_len;
    ssize_t comp_len;
}sock_data_t;

typedef struct _sock_opt_t {
    int level;
    int optname;
    const char *optval;
    int len;
}sock_opt_t;

//通用参数结构
typedef struct _sock_req_t {
    int sockfd;
    union {
        sock_create_t create;
        sock_data_t data;
        sock_opt_t opt;
        sock_conn_t conn;
        sock_bind_t bind;
    };
    sock_wait_t *wait;
    int wait_tmo;

} sock_req_t;
net_err_t socket_init (void);

net_err_t sock_create_req_in (struct _func_msg_t *msg);
net_err_t sock_bind_req_in (struct _func_msg_t *msg);
net_err_t sock_connect_req_in (struct _func_msg_t *msg);
net_err_t sock_sendto_req_in (struct _func_msg_t *msg);
net_err_t sock_send_req_in (struct _func_msg_t *msg);
net_err_t sock_recvfrom_req_in (struct _func_msg_t *msg);
net_err_t sock_recv_req_in (struct _func_msg_t *msg);
net_err_t sock_setsockopt_req_in (struct _func_msg_t *msg);
net_err_t sock_close_req_in (struct _func_msg_t *msg);

//sock初始化
net_err_t sock_init (sock_t *sock, int family, int protocol, const sock_ops_t *ops);

net_err_t sock_uninit (sock_t *sock);

//通用函数
net_err_t sock_setopt (struct _sock_t* s,  int level, int optname, const char * optval, int optlen);
net_err_t sock_connect (struct _sock_t* s, const struct x_sockaddr* addr, x_socklen_t len);
net_err_t sock_bind (struct _sock_t* s, const struct x_sockaddr* addr, x_socklen_t len);
net_err_t sock_send (struct _sock_t * s, const void* buf, size_t len, int flags, ssize_t * result_len);
net_err_t sock_recv (struct _sock_t * s, void* buf, size_t len, int flags, ssize_t * result_len);

void sock_wakeup (sock_t *sock, int type, int err);


#endif