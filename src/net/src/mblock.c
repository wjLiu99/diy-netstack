#include "mblock.h"
#include "dbg.h"
#include "net_cfg.h"

net_err_t mblock_init (mblock_t *mblock, void *mem, int blk_size, int cnt, nlocker_type_t type){
    uint8_t *buf = (uint8_t *)mem;

    nlist_init(&mblock->free_list);
    mblock->start = mem;
    for(int i = 0; i < cnt; i++, buf += blk_size){
        nlist_node_t *block = (nlist_node_t *)buf;
        nlist_node_init(block);
        nlist_insert_last(&mblock->free_list, block);
    }

    nlocker_init(&mblock->locker, type);


    if(type != NLOCKER_NONE){
        mblock->alloc_sem = sys_sem_create(cnt);
        if(mblock->alloc_sem == SYS_SEM_INVALID){
            dbg_error(DBG_MBLOCK,"sem create err");
            
            nlocker_destory(&mblock->locker);
            return NET_ERR_SYS;
        }
    }
    return NET_ERR_OK;

}

void *mblock_alloc (mblock_t *block, int ms){
    if((ms < 0) || (block->locker.type == NLOCKER_NONE)){
        nlocker_lock(&block->locker);
        int cnt = nlist_count(&block->free_list);
        nlocker_unlock(&block->locker);
        if(cnt == 0){
            
            return (void *)0;
        }
    }

    if(block->locker.type != NLOCKER_NONE){
        sys_sem_wait(block->alloc_sem, ms);
       
    } 
   
    nlocker_lock(&block->locker);
    nlist_node_t *node = nlist_remove_first(&block->free_list);
    nlocker_unlock(&block->locker);
    return (void *)node;
    

    
}

int mblock_free_cnt (mblock_t *block){
    nlocker_lock(&block->locker);
    int cnt = nlist_count(&block->free_list);
    nlocker_unlock(&block->locker);
    return cnt;
}


void mblock_free(mblock_t* mblock, void* block) {
    nlocker_lock(&mblock->locker);
    nlist_insert_last(&mblock->free_list, (nlist_node_t *)block);
    nlocker_unlock(&mblock->locker);

    
    if (mblock->locker.type != NLOCKER_NONE) {
        sys_sem_notify(mblock->alloc_sem);
    }
}


void mblock_destory(mblock_t *block){
    if(block->locker.type != NLOCKER_NONE){
        sys_sem_free(block->alloc_sem);
        nlocker_destory(&block->locker);
    }
}