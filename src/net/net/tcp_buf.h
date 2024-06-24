#ifndef TCP_BUF_H
#define TCP_BUF_H

#include <stdint.h>
#include "pktbuf.h"
typedef struct _tcp_buf_t {
    uint8_t *data;
    int count;
    int size;
    int in, out;

}tcp_buf_t;

void tcp_buf_init (tcp_buf_t *buf, uint8_t *data, int size);

//写入发送缓冲区
void tcp_buf_write_send (tcp_buf_t *buf, const uint8_t *data, int len);
//读发送缓冲区，拷贝数据到dest中，偏移量为offset，拷贝数据长度len
void tcp_buf_read_send (tcp_buf_t *buf, int offset, pktbuf_t *dest, int len);
//移除缓冲区数据，接受到对方ack调用
int tcp_buf_remove (tcp_buf_t *buf, int cnt);

static inline int tcp_buf_size (tcp_buf_t *buf) {
    return buf->size;
}

static inline int tcp_buf_count (tcp_buf_t *buf) {
    return buf->count;


}

static inline int tcp_buf_free_cnt (tcp_buf_t *buf) {
    return buf->size - buf->count;
}

#endif