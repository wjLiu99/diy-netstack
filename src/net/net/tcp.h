#ifndef TCP_H
#define TCP_H

#include "sock.h"
#include "net_cfg.h"
#include "tcp_buf.h"

#define TCP_OPT_END     0
#define TCP_OPT_NOP     1
#define TCP_OPT_MSS     2

#define TCP_DEFAULT_MSS     536

#pragma pack(1)

typedef struct _tcp_opt_mss_t {
    uint8_t kind;
    uint8_t length;
    uint16_t mss;
}tcp_opt_mss_t;

typedef struct _tcp_hdr_t {
    uint16_t sport;
    uint16_t dport;
    uint32_t seq;
    uint32_t ack;
    union {
        uint16_t flags;
#if NET_ENDIAN_LITTLE
        struct {
            uint16_t resv : 4;          // 保留
            uint16_t shdr : 4;          // 头部长度
            uint16_t f_fin : 1;           // 已经完成了向对方发送数据，结束整个发送
            uint16_t f_syn : 1;           // 同步，用于初始一个连接的同步序列号
            uint16_t f_rst : 1;           // 重置连接
            uint16_t f_psh : 1;           // 推送：接收方应尽快将数据传递给应用程序
            uint16_t f_ack : 1;           // 确认号字段有效
            uint16_t f_urg : 1;           // 紧急指针有效
            uint16_t f_ece : 1;           // ECN回显：发送方接收到了一个更早的拥塞通告
            uint16_t f_cwr : 1;           // 拥塞窗口减，发送方降低其发送速率
        };
#else
        struct {
            uint16_t shdr : 4;          // 头部长度
            uint16_t resv : 4;          // 保留
            uint16_t f_cwr : 1;           // 拥塞窗口减，发送方降低其发送速率
            uint16_t f_ece : 1;           // ECN回显：发送方接收到了一个更早的拥塞通告
            uint16_t f_urg : 1;           // 紧急指针有效
            uint16_t f_ack : 1;           // 确认号字段有效
            uint16_t f_psh : 1;           // 推送：接收方应尽快将数据传递给应用程序
            uint16_t f_rst : 1;           // 重置连接
            uint16_t f_syn : 1;           // 同步，用于初始一个连接的同步序列号
            uint16_t f_fin : 1;           // 已经完成了向对方发送数据，结束整个发送
        };
#endif
    };
    uint16_t win;                       // 窗口大小，实现流量控制, 窗口缩放选项可以提供更大值的支持
    uint16_t checksum;                  // 校验和
    uint16_t urgptr;                    // 紧急指针
}tcp_hdr_t;


typedef struct _tcp_pkt_t {
    tcp_hdr_t hdr;
    uint8_t data[1];
}tcp_pkt_t;


#pragma pack()
//tcp状态机
typedef enum _tcp_state_t {
    TCP_STATE_FREE = 0,             // 空闲状态，非标准状态的一部分
    TCP_STATE_CLOSED,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECVD,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSING,
    TCP_STATE_TIME_WAIT,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,

    TCP_STATE_MAX,
}tcp_state_t;

//tcp报文段结构，把收到的tcp数据包信息保存起来
typedef struct _tcp_seg_t {
    ipaddr_t local_ip;      //本地ip
    ipaddr_t remote_ip;     //远端ip    
    tcp_hdr_t *hdr;         //tcp数据包
    pktbuf_t *buf;          
    uint32_t data_len;      //数据长度
    uint32_t seq;           //数据包起始序号
    uint32_t seq_len;       //数据包长度
} tcp_seg_t;

//tcp连接控制块
typedef struct _tcp_t {
    sock_t base;
    tcp_state_t state;      // TCP状态
    int mss;
    //连接相关
    struct {
        sock_wait_t wait;
    } conn;

    //发送相关
    struct {
        uint32_t una;   //未确认的第一个字节
        uint32_t nxt;   //下一个待发送的字节
        uint32_t iss;   //初始序号
        sock_wait_t wait;

        tcp_buf_t buf;
        uint8_t data[TCP_SBUF_SIZE];
    } send;

    //接收相关
    struct {
        uint32_t nxt;   //期望收到的下一个字节
        uint32_t iss;   //初始序号
        sock_wait_t wait;
    } recv;

    //标志位，记录数据发送情况，处理重传
    struct {
        uint32_t syn_out : 1;   //syn是否发送
        uint32_t fin_out : 1;   //fin是否发送
        uint32_t irs_valid : 1; //是否收到对方syn
    } flags;

} tcp_t;

net_err_t tcp_init (void);

sock_t *tcp_create (int family, int protocol);

tcp_t * tcp_find (ipaddr_t *local_ip, uint16_t local_port, ipaddr_t *remote_ip, uint16_t remote_port);


void tcp_read_option (tcp_t *tcp, tcp_hdr_t *hdr);

//终止tcp连接
net_err_t tcp_abort(tcp_t *tcp, net_err_t err);

#if DBG_DISPLAY_ENABLED(DBG_TCP)       
void tcp_show_info (char * msg, tcp_t * tcp);
void tcp_display_pkt (char * msg, tcp_hdr_t * tcp_hdr, pktbuf_t * buf);
void tcp_show_list (void);
#else
#define tcp_show_info(msg, tcp)
#define tcp_display_pkt(msg, hdr, buf)
#define tcp_show_list()
#endif


static inline int tcp_hdr_size (tcp_hdr_t *hdr) {
    return hdr->shdr * 4;
}


static inline void tcp_set_hdr_size (tcp_hdr_t *hdr, int size) {
    hdr->shdr  = size / 4;
}
#endif