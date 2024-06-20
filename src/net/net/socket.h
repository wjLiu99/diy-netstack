#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include "net_cfg.h"
#include "ntools.h"
#include "sock.h"

#undef AF_INET
#define AF_INET                 0               // IPv4协议簇


#undef SOCK_RAW
#define SOCK_RAW           1              // 原始数据报
// #undef SOCK_DGRAM
// #define SOCK_DGRAM              2               // 数据报式套接字
// #undef SOCK_STREAM
// #define SOCK_STREAM             3               // 流式套接字

#undef IPPROTO_ICMP
#define IPPROTO_ICMP            1               // ICMP协议
// #undef IPPROTO_UDP
// #define IPPROTO_UDP             17              // UDP协议

// #undef IPPROTO_TCP
// #define IPPROTO_TCP             6               // TCP协议

#undef INADDR_ANY
#define INADDR_ANY              0               // 任意IP地址，即全0的地址

#pragma pack(1)
struct x_in_addr {
    union{
        struct {
            uint8_t addr0;
            uint8_t addr1;
            uint8_t addr2;
            uint8_t addr3;
        };
        uint8_t addr_array[IPV4_ADDR_SIZE];
        #undef s_addr
        uint32_t s_addr;
    };
};

//通用地址结构
struct x_sockaddr {
    uint8_t sa_len;                 // 整个结构的长度，值固定为16
    uint8_t sa_family;              // 地址簇：NET_AF_INET
    uint8_t sa_data[14];            // 数据空间
};

//ipv4地址结构
struct x_sockaddr_in {
    uint8_t sin_len;                // 整个结构的长度，值固定为16
    uint8_t sin_family;             // 地址簇：AF_INET
    uint16_t sin_port;              // 端口号
    struct x_in_addr sin_addr;      // IP地址
    char sin_zero[8];               // 填充字节
};
#pragma pack()


int x_socket (int family, int type, int protocol);
ssize_t x_sendto(int s, const void* buf, size_t len, int flags, const struct x_sockaddr* dest, x_socklen_t dest_len);
ssize_t x_recvfrom(int s, void* buf, size_t len, int flags, struct x_sockaddr* src, x_socklen_t* src_len);
#endif