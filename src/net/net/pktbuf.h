#ifndef PKTBUF_H
#define PKTBUF_H
#include "nlist.h"
#include "net_cfg.h"
#include "sys.h"
#include "net_err.h"

//数据块结构体
typedef struct _pktblk_t{
    nlist_node_t node;
    int size;
    uint8_t *data;
    uint8_t payload[PKTBUF_BLK_SIZE];

}pktblk_t;

//数据包结构体
typedef struct _pktbuf_t{
    int total_size;
    nlist_t blk_list;
    nlist_node_t node;
    int ref;

    //读写控制
    int pos;    
    pktblk_t *cur_blk;      //当前块
    uint8_t *blk_offset;    //块内偏移
}pktbuf_t;

net_err_t pktbuf_init (void);

pktbuf_t * pktbuf_alloc (int size);

void pktbuf_free (pktbuf_t *buf);

//在pktbuf头部添加数据块，是否要求连续
net_err_t pktbuf_add_header(pktbuf_t *buf, int size, int cont);
//移除包头
net_err_t pktbuf_remove_header(pktbuf_t *buf, int size);
//调整数据包大小,在数据包尾部调整，数据块头插法
net_err_t pktbuf_resize (pktbuf_t *buf, int size);

//合并两个数据包，保留destpktbuf
net_err_t pktbuf_join (pktbuf_t *dest, pktbuf_t * src);

//调整包头的连续性
net_err_t pktbuf_set_cont (pktbuf_t *buf, int size);

//重置pktbuf读写位置
void pktbuf_reset_acc (pktbuf_t *buf);

//写入数据包
int pktbuf_write (pktbuf_t *buf, uint8_t *src, int size);

//读取数据包
int pktbuf_read (pktbuf_t *buf, uint8_t *dest, int size);

//读写定位
net_err_t pktbuf_seek (pktbuf_t *buf, int offset);

//数据包拷贝
net_err_t pktbuf_copy (pktbuf_t *dest, pktbuf_t *src, int size);

//填充数据包
int pktbuf_fill (pktbuf_t *buf, uint8_t v, int size);

//增加数据包引用计数
void pktbuf_inc_ref (pktbuf_t *buf);
//不连续的数据计算校验和
uint16_t pktbuf_checksum16 (pktbuf_t *buf, uint32_t len, uint32_t pre_sum, int complement);



static inline pktblk_t *pktblk_blk_next(pktblk_t *blk){
    nlist_node_t *next = nlist_node_next(&blk->node);
    return nlist_entry(next, pktblk_t, node);
}

static inline pktblk_t *pktbuf_first_blk(pktbuf_t *buf){
    nlist_node_t *first = nlist_first(&buf->blk_list);
    return nlist_entry(first, pktblk_t, node);
}

static inline pktblk_t *pktbuf_last_blk(pktbuf_t *buf){
    nlist_node_t *last = nlist_last(&buf->blk_list);
    return nlist_entry(last, pktblk_t, node);
}

static inline int pktbuf_total(pktbuf_t *buf){
    return buf->total_size;
}

//数据包数据部分起始地址
static inline uint8_t *pktbuf_data(pktbuf_t *buf) {
    pktblk_t *blk = pktbuf_first_blk(buf);
    return blk ? blk->data : (uint8_t *)0;
}
#endif