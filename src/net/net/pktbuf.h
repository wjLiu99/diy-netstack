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
}pktbuf_t;

net_err_t pktbuf_init (void);
#endif