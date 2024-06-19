#include "pktbuf.h"
#include "dbg.h"
#include "mblock.h"
#include "nlocker.h"
#include "ntools.h"

static nlocker_t locker;

static pktblk_t block_buffer[PKTBUF_BLK_CNT];
static pktbuf_t pktbuf_buffer[PKTBUF_BUF_CNT];

static mblock_t block_list;
static mblock_t pktbuf_list;
//当前读写位置后面还有多少空间
static inline int total_blk_remain(pktbuf_t *buf) {
    return buf->total_size - buf->pos;
}

//当前块中还有多少空间
static int cur_blk_remain(pktbuf_t *buf) {
    pktblk_t *blk = buf->cur_blk;
    if (!blk) {
        return 0;
    }

    return (int)(buf->cur_blk->data + blk->size - buf->blk_offset);
}

//调整读写位置
static void move_forward (pktbuf_t *buf, int size) {
    buf->pos += size;
    buf->blk_offset += size;

    if (buf->blk_offset >= (buf->cur_blk->data + buf->cur_blk->size)){
        buf->cur_blk = pktblk_blk_next(buf->cur_blk);
        if (buf->cur_blk) {
            buf->blk_offset = buf->cur_blk->data;
        } else {
            buf->blk_offset = (uint8_t *)0;
        }
        
    }
}
//当前数据块的空闲空间
static inline int cur_blk_tail_free(pktblk_t *blk) {
    return (int)((blk->payload + PKTBUF_BLK_SIZE) - (blk->data + blk->size));
}
//释放数据块
static void pktblock_free(pktblk_t *block) {
    mblock_free(&block_list, block);
}
//释放数据块链
static void pktblock_free_list (pktblk_t *first) {
    while (first) {
        pktblk_t *next_block = pktblk_blk_next(first);
        pktblock_free(first);
        first = next_block;
    }
}




#if DBG_DISPLAY_ENABLED(DBG_PKTBUF)
static void display_check_buf(pktbuf_t *buf){
    if(!buf) {
        dbg_error(DBG_PKTBUF, "invailed buf, buf == 0");
        return;
    }
    plat_printf("check buf %p: size: %d\n",buf, buf->total_size);
    pktblk_t *cur;
    int index = 0;
    int total_size = 0;
    for (cur = pktbuf_first_blk(buf); cur; cur = pktblk_blk_next(cur)) {
        plat_printf("%d: ",index++);
        if((cur->data < cur->payload) || (cur->data >=cur->payload + PKTBUF_BLK_SIZE)){
            dbg_error(DBG_PKTBUF, "buf data err");
        }

        int pre_size = (int)(cur->data - cur->payload);
        plat_printf("pre: %d b, ", pre_size);
        int used_size = cur->size;
        plat_printf("used: %d b, ",used_size);
        int free_size = cur_blk_tail_free(cur);
        plat_printf("free: %d b, \n",free_size);
        int blk_total = pre_size + used_size + free_size;
        if (blk_total != PKTBUF_BLK_SIZE){
            dbg_error(DBG_PKTBUF, "bad block size");
        }
        total_size += used_size;
        



    }
    if (total_size != buf->total_size){
            dbg_error(DBG_PKTBUF, "total size err");
        }
}
#else
#define display_check_buf(buf)
#endif

net_err_t pktbuf_init (void){
    dbg_info(DBG_PKTBUF, "pktbuf init");

    nlocker_init(&locker, NLOCKER_THREAD);
    mblock_init(&block_list, block_buffer, sizeof(pktblk_t), PKTBUF_BLK_CNT, NLOCKER_NONE);
    mblock_init(&pktbuf_list, pktbuf_buffer, sizeof(pktbuf_t), PKTBUF_BUF_CNT, NLOCKER_NONE);

    dbg_info(DBG_PKTBUF, "pktbuf init done");
    return NET_ERR_OK;
}

//数据块分配
static pktblk_t *pktblock_alloc(void){
    nlocker_lock(&locker);
    //pktbuf分配可能被中断程序调用，不应该等
    pktblk_t *block = mblock_alloc(&block_list, -1);
    nlocker_unlock(&locker);
    if(block){
        block->size = 0;
        block->data = (uint8_t *)0;
        nlist_node_init(&block->node);
    }

    return block;
}
//创建数据块链表 0:尾插 1：头插
static pktblk_t *pktblock_alloc_list(int size, int add_front){
    pktblk_t *first_block = (pktblk_t *)0;
    pktblk_t *pre_block = (pktblk_t *)0;
    while (size) {
        pktblk_t *new_block = pktblock_alloc();
        if(!new_block){
            dbg_error(DBG_PKTBUF, "no buf alloc");
            if (!first_block) {
                pktblock_free_list(first_block);
            }
           
            return (pktblk_t *)0;
        }

        int cur_size = 0;
        if (add_front) {
            cur_size = size > PKTBUF_BLK_SIZE ? PKTBUF_BLK_SIZE : size;
            new_block->size = cur_size;
            new_block->data = new_block->payload + PKTBUF_BLK_SIZE - cur_size;

            if(first_block){
                nlist_node_set_next(&new_block->node, &first_block->node);
            }

            first_block = new_block;
        }else{
            if (!first_block) {
                first_block = new_block;
            }

            cur_size = size > PKTBUF_BLK_SIZE ? PKTBUF_BLK_SIZE : size;
            new_block->size = cur_size;
            new_block->data = new_block->payload;
            if(pre_block){
                nlist_node_set_next(&pre_block->node, &new_block->node);
            }

        }
        size -= cur_size;
        pre_block = new_block;

    }
    return first_block;
}

//1:尾插 0：头插
static void pktbuf_insert_blk_list(pktbuf_t *buf, pktblk_t *first_block, int add_list){
    //尾插
    if (add_list) {
        while (first_block) {
            pktblk_t *next_block = pktblk_blk_next(first_block);

            nlist_insert_last(&buf->blk_list, &first_block->node);
            buf->total_size += first_block->size;
            first_block = next_block;
        }
    }else {
        //头插
        pktblk_t *pre = (pktblk_t *)0;
        while (first_block){
            pktblk_t *next_block = pktblk_blk_next(first_block);
            if(pre) {
                nlist_insert_after(&buf->blk_list, &pre->node, &first_block->node); 
            } else {
                nlist_insert_first(&buf->blk_list, &first_block->node);
            }

            buf->total_size += first_block->size;
            pre = first_block;
            first_block = next_block;

        } 
    }
}
//分配pktbuf
pktbuf_t *pktbuf_alloc (int size){
    nlocker_lock(&locker);
    pktbuf_t *buf = mblock_alloc(&pktbuf_list, -1);
    nlocker_unlock(&locker);
    if (!buf) {
        dbg_error(DBG_PKTBUF, "no buffer");
        return (pktbuf_t *)0;
    }
    buf->ref = 1;
    buf->total_size = 0;
    nlist_init(&buf->blk_list);
    nlist_node_init(&buf->node);


    if (size) {
        //分配数据块链
        pktblk_t *block = pktblock_alloc_list(size, 1);
        if (!block) {
            nlocker_lock(&locker);
            mblock_free(&pktbuf_list, buf);
            nlocker_unlock(&locker);
            return (pktbuf_t *)0;
        }
        //将分配的数据块链整体插入pktbuf链表中
        pktbuf_insert_blk_list(buf, block, 1);
    }
    pktbuf_reset_acc(buf);
    display_check_buf(buf);
    return buf; 
}
//释放pktbuf
void pktbuf_free (pktbuf_t *buf){
    nlocker_lock(&locker);
    if(--buf->ref == 0){
        pktblock_free_list(pktbuf_first_blk(buf));
        mblock_free(&pktbuf_list, buf);
    }
    nlocker_unlock(&locker);

}

net_err_t pktbuf_add_header(pktbuf_t *buf, int size, int cont){

    pktblk_t *block = pktbuf_first_blk(buf);

    int resv_size = (int)(block->data - block->payload);
    if(size <= resv_size) {
        block->data -= size;
        block->size += size;
        buf->total_size += size;
        display_check_buf(buf);
        return NET_ERR_OK;
    }

    if (cont) {
        if(size > PKTBUF_BLK_SIZE) {
            dbg_error(DBG_PKTBUF, "set cont, size too big");
            return NET_ERR_SIZE;
        }
        block = pktblock_alloc_list(size, 1);
        if(!block) {
            dbg_error(DBG_PKTBUF, "no buf");
            return NET_ERR_SIZE;
        }

    } else {
        block->data = block->payload;
        block->size += resv_size;
        buf->total_size += resv_size;
        size -= resv_size;

        block = pktblock_alloc_list(size, 1);
         if(!block) {
            dbg_error(DBG_PKTBUF, "no buf");
            return NET_ERR_SIZE;
        }
        pktbuf_insert_blk_list(buf, block, 0);
        display_check_buf(buf);
        return NET_ERR_OK;
    }
    pktbuf_insert_blk_list(buf, block, 0);
    display_check_buf(buf);
    return NET_ERR_OK;


}

net_err_t pktbuf_remove_header(pktbuf_t *buf, int size){
    pktblk_t *block = pktbuf_first_blk(buf);
    while (size) {
        pktblk_t *next_block = pktblk_blk_next(block);
        if (size < block->size) {
            block->data += size;
            block->size -= size;
            buf->total_size -= size;
            break;
        }

        int cur_size = block->size;
        nlist_remove_first(&buf->blk_list);
        pktblock_free(block);
        size -= cur_size;
        buf->total_size -= cur_size;
        block = next_block;
    }
    display_check_buf(buf);
    return NET_ERR_OK;
}

net_err_t pktbuf_resize (pktbuf_t *buf, int size){
    if (size == buf->total_size){
        return NET_ERR_OK;
    }

    if(buf->total_size == 0){
        pktblk_t *block = pktblock_alloc_list(size, 0);
        if(!block){
            dbg_error(DBG_PKTBUF, "no block");
            return NET_ERR_MEM;
        }
        pktbuf_insert_blk_list(buf, block, 1);
    } else if (size > buf->total_size) {
        pktblk_t *tail_blk = pktbuf_last_blk(buf);

        int incsize = size - buf->total_size;
        int remain_size = cur_blk_tail_free(tail_blk);
        if (remain_size >= incsize) {
            tail_blk->size += incsize;
            buf->total_size += incsize;
        } else {
            pktblk_t *new_blk = pktblock_alloc_list(incsize - remain_size, 0);
            if(!new_blk) {
                dbg_error(DBG_PKTBUF, "no block");
                return NET_ERR_MEM;
            }
            tail_blk->size += remain_size;
            buf->total_size += remain_size;
            pktbuf_insert_blk_list(buf, new_blk, 1);

        }
    } else if (size == 0){
        pktblock_free_list(pktbuf_first_blk(buf));
        buf->total_size = 0;
        nlist_init(&buf->blk_list);
    } else {
        int total_size = 0;
        pktblk_t *tail_blk;
        for (tail_blk = pktbuf_first_blk(buf); tail_blk; tail_blk = pktblk_blk_next(tail_blk)){
            total_size += tail_blk->size;
            if (total_size >= size) {
                break;
            }
        }

        if(tail_blk == (pktblk_t *)0) {
            return NET_ERR_SIZE;
        }
        pktblk_t *cur_blk = pktblk_blk_next(tail_blk);
        while (cur_blk) {
            pktblk_t *next = pktblk_blk_next(cur_blk);
            buf->total_size -= cur_blk->size;
            nlist_remove(&buf->blk_list, &cur_blk->node);
           
            pktblock_free(cur_blk);
            cur_blk = next;
        }
        buf->total_size -= (total_size - size);
        tail_blk->size -= (total_size - size);
    }
    
    display_check_buf(buf);
    return NET_ERR_OK;
}

net_err_t pktbuf_join (pktbuf_t *dest, pktbuf_t * src){
    pktblk_t *first;
    while ((first = pktbuf_first_blk(src))) {
        nlist_remove_first(&src->blk_list);
        pktbuf_insert_blk_list(dest, first, 1);

    }
    pktbuf_free(src);
    display_check_buf(dest);
    return NET_ERR_OK;
}


net_err_t pktbuf_set_cont (pktbuf_t *buf, int size){
    if (size > buf->total_size || size > PKTBUF_BLK_SIZE) {
        dbg_error(DBG_PKTBUF, "size err");
        return NET_ERR_SIZE;
    }

    pktblk_t *first = pktbuf_first_blk(buf);
    if (size <= first->size) {
        display_check_buf(buf);
        return NET_ERR_OK;
    }

    uint8_t *dest = first->payload;
    for (int i = 0; i < first->size; i++){
        *dest++ = first->data[i];
    }
    first->data = first->payload;

    int remain_size = size - first->size;
    pktblk_t *next = pktblk_blk_next(first);

    while (remain_size && next){
        int cur_size = (remain_size > next->size) ? next->size : remain_size;
        for (int i = 0; i < cur_size; i++){
            *dest++ = next->data[i];
        }

        next->size -= cur_size;
        next->data += cur_size;
        first->size += cur_size;
        remain_size -= cur_size;

        if (next->size == 0) {
            pktblk_t *blk = pktblk_blk_next(next);
            nlist_remove(&buf->blk_list, &next->node);
            pktblock_free(next);
            next = blk;
        }
    }

    display_check_buf(buf);
    return NET_ERR_OK;

}

void pktbuf_reset_acc (pktbuf_t *buf){
    if (buf){
        buf->pos = 0;
        buf->cur_blk = pktbuf_first_blk(buf);
        buf->blk_offset = buf->cur_blk ? buf->cur_blk->data : (uint8_t *)0;
    }
}

int pktbuf_write (pktbuf_t *buf, uint8_t *src, int size){
    if (!src || !size) {
        return NET_ERR_PARAM;
    }

    int remain_size = total_blk_remain(buf);
    if(remain_size < size) {
        dbg_error(DBG_PKTBUF, "size err");
        return NET_ERR_SIZE;
    }

    while (size) {
        int blk_size = cur_blk_remain(buf);
        int cur_size = (size > blk_size) ? blk_size : size;
        plat_memcpy(buf->blk_offset, src, cur_size);

        src += cur_size;
        size -= cur_size;
        move_forward(buf,cur_size);

    }
    return NET_ERR_OK;

}
int pktbuf_read (pktbuf_t *buf, uint8_t *dest, int size){
    if (!dest || !size) {
        return NET_ERR_PARAM;
    }

    int remain_size = total_blk_remain(buf);
    if(remain_size < size) {
        dbg_error(DBG_PKTBUF, "size err");
        return NET_ERR_SIZE;
    }

    while (size) {
        int blk_size = cur_blk_remain(buf);
        int cur_size = (size > blk_size) ? blk_size : size;
        plat_memcpy(dest, buf->blk_offset, cur_size);

        dest += cur_size;
        size -= cur_size;
        move_forward(buf,cur_size);

    }
    return NET_ERR_OK;
}


net_err_t pktbuf_seek (pktbuf_t *buf, int offset){
    if (buf->pos == offset) {
        return  NET_ERR_OK;
    }

    if ((offset < 0) || (offset >= buf->total_size)){
        return NET_ERR_PARAM;
    }



    int move;
    //往前移动，调整为从开头开始往后移
    if (offset < buf->pos) {
        buf->cur_blk = pktbuf_first_blk(buf);
        buf->pos = 0;
        buf->blk_offset = buf->cur_blk->data;
        move = offset;
    } else {
        move = offset - buf->pos;
    }
    while (move) {
        int remain_size = cur_blk_remain(buf);
        int cur_size = move > remain_size ? remain_size : move;

        move_forward(buf, cur_size);
        move -= cur_size;
    }
    return NET_ERR_OK;

}

net_err_t pktbuf_copy (pktbuf_t *dest, pktbuf_t *src, int size){
    if ((total_blk_remain(dest) < size ) || (total_blk_remain(src) < size)){
        return NET_ERR_SIZE;
    }
    while (size) {
        int dest_remain = cur_blk_remain(dest);
        int src_remain = cur_blk_remain(src);
        int cur_size = dest_remain > src_remain ? src_remain : dest_remain;

        cur_size = cur_size > size ? size : cur_size;

        plat_memcpy(dest->blk_offset, src->blk_offset, cur_size);

        move_forward(dest, cur_size);
        move_forward(src, cur_size);
        size -= cur_size;
    }
    return NET_ERR_OK;
}


int pktbuf_fill (pktbuf_t *buf, uint8_t v, int size){
      if (!size) {
        return NET_ERR_SIZE;
    }

    int remain_size = total_blk_remain(buf);
    if(remain_size < size) {
        dbg_error(DBG_PKTBUF, "size err");
        return NET_ERR_SIZE;
    }

    while (size) {
        int blk_size = cur_blk_remain(buf);
        int cur_size = (size > blk_size) ? blk_size : size;
        plat_memset(buf->blk_offset, v, cur_size);
        size -= cur_size;
        move_forward(buf,cur_size);

    }
    return NET_ERR_OK;

}

void pktbuf_inc_ref (pktbuf_t *buf){
    nlocker_lock(&locker);
    buf->ref++;
    nlocker_unlock(&locker);
}


uint16_t pktbuf_checksum16 (pktbuf_t *buf, uint32_t size, uint32_t pre_sum, int complement) {
    int remain_size = total_blk_remain(buf);
    if (remain_size < size) {
        dbg_warning(DBG_PKTBUF, "size too big");
        return NET_ERR_SIZE;
    }

    uint32_t sum = pre_sum;
    while (size > 0) {
        int blk_size = cur_blk_remain(buf);
        int cur_size = size > blk_size ? blk_size : size;

        sum = checksum16(buf->blk_offset, cur_size, sum, 0);
        size -= cur_size;
        move_forward(buf, cur_size);
    }
    return complement ? (uint16_t)~sum : (uint16_t)sum;
}