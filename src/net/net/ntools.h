#ifndef NTOOLS_H
#define NTOOLS_H
#include "net_err.h"
#include <stdint.h>
#include "net_cfg.h"
//16位大小端转换
static inline uint16_t swap_u16 (uint16_t v) {
    uint16_t r = (((v & 0xff) << 8) | ((v >> 8) & 0xff));
    return r;
}

//32位大小端转换
static inline uint32_t swap_u32 (uint32_t v) {
    uint32_t r = (
        (((v >> 0) & 0xff) << 24) |
        (((v >> 8) & 0xff) << 16) |
        (((v >> 16) & 0xff) << 8) |
        (((v >> 24) & 0xff) << 0)
    );
    return r;
}

#define x_htons(v)  swap_u16(v)
#define x_ntohs(v)  swap_u16(v)
#define x_htonl(v)  swap_u32(v)
#define x_ntohl(v)  swap_u32(v)

net_err_t tools_init(void);
#endif