#ifndef MBLOCK_H
#define MBLOCK_H

#include "nlist.h"
#include "nlocker.h"
#include "net_err.h"

typedef struct _mblock_t{
    nlist_t free_list;         //空闲链表
    void *start;            //内存块起始地址
    nlocker_t locker;       //线程互斥锁
    sys_sem_t alloc_sem;    //信号量
}mblock_t;

//初始化内存分配结构
net_err_t mblock_init (mblock_t *mblock, void *mem, int blk_size, int cnt, nlocker_type_t type);
//内存分配，ms为是否等待，小于0不等，0阻塞等，大于0等待单位毫秒
void *mblock_alloc (mblock_t *block, int ms);
//内存释放
void mblock_free(mblock_t* mblock, void* block);
//空闲内存块数量
int mblock_free_cnt (mblock_t *block);
//销毁内存分配结构
void mblock_destory(mblock_t *block);
#endif