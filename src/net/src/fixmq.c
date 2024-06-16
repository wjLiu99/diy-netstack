#include "fixmq.h"
#include "dbg.h"
#include "net_cfg.h"
net_err_t fixmq_init (fixmq_t *mq, void  **buf, int size, nlocker_type_t type){

    mq->size = size;
    mq->buf = buf;
    mq->in = mq->out = mq->cnt =0;
    mq->recv_sem = mq->send_sem = SYS_SEM_INVALID;
    net_err_t err = nlocker_init(&mq->locker, type);
    if(err < 0){
        dbg_error(DBG_FIXMQ, "init locker failed");
        return err;
    }

    mq->send_sem = sys_sem_create(size);
    if(mq->send_sem == SYS_SEM_INVALID){
        dbg_error(DBG_FIXMQ, "create sem failed");
        err = NET_ERR_SYS;
        goto init_failed;
    }

    mq->recv_sem = sys_sem_create(0);
    if(mq->recv_sem == SYS_SEM_INVALID){
        dbg_error(DBG_FIXMQ, "create sem failed");
        err = NET_ERR_SYS;
        goto init_failed;
    }
    mq->buf = buf;
    return NET_ERR_OK;

init_failed:
    if(mq->send_sem != SYS_SEM_INVALID){
        sys_sem_free(mq->send_sem);
    }   
    if(mq->recv_sem != SYS_SEM_INVALID){
        sys_sem_free(mq->recv_sem);
    }   
    
    nlocker_destory(&mq->locker);
    return err;

}


net_err_t fixmq_send(fixmq_t *mq, void *msg, int tmo){
    nlocker_lock(&mq->locker);
    if((tmo < 0) && (mq->cnt >= mq->size)){
        nlocker_unlock(&mq->locker);
        return NET_ERR_FULL;
    }
    nlocker_unlock(&mq->locker);

    if(sys_sem_wait(mq->send_sem, tmo) < 0){
        return NET_ERR_TMO;
    }
    nlocker_lock(&mq->locker);
    mq->buf[mq->in++] = msg;
    if(mq->in >= mq->size){
        mq->in = 0;
    }
    mq->cnt++;

    nlocker_unlock(&mq->locker);

    sys_sem_notify(mq->recv_sem);
    return NET_ERR_OK;
}

void *fixmq_recv (fixmq_t *mq, int tmo){
    nlocker_lock(&mq->locker);
    if(!mq->cnt && (tmo < 0)){
        nlocker_unlock(&mq->locker);
        return (void *)0;
    }

    nlocker_unlock(&mq->locker);

    if(sys_sem_wait(mq->recv_sem, tmo) < 0){
        return (void *)0;
    }

    //取出消息
    nlocker_lock(&mq->locker);
    void *msg = mq->buf[mq->out++];
    if(mq->out >= mq->size){
        mq->out = 0;
    }
    mq->cnt--;

    nlocker_unlock(&mq->locker);

    sys_sem_notify(mq->send_sem);
    return msg;
}


void fixmq_destory(fixmq_t *mq){
    nlocker_destory(&mq->locker);
    sys_sem_free(mq->send_sem);
    sys_sem_free(mq->recv_sem);

}
int fixmq_msg_count(fixmq_t *mq){
    nlocker_lock(&mq->locker);
    int count = mq->cnt;

    nlocker_unlock(&mq->locker);
    return count;
}