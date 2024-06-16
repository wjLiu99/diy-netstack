#ifndef FIXMQ_H
#define FIXMQ_H
#include "nlocker.h"
#include "sys.h"
#include "net_err.h"

//定长消息队列
typedef struct _fixmq_t{
    int size;           //消息队列长度
    int in, out, cnt;   //读写指针，消息数量
    void **buf;         //消息起始地址

    nlocker_t locker;   //互斥
    sys_sem_t recv_sem; //读信号量
    sys_sem_t send_sem; //写信号量

}fixmq_t;

//消息队列初始化
net_err_t fixmq_init (fixmq_t *mq, void  **buf, int size, nlocker_type_t type);
//向消息队列发送消息,msg消息结构，tmo是否等待
net_err_t fixmq_send (fixmq_t *mq, void *msg, int tmo);

//从消息队列中取出消息
void *fixmq_recv (fixmq_t *mq, int tmo);

void fixmq_destory(fixmq_t *mq);
int fixmq_msg_count(fixmq_t *mq);
#endif