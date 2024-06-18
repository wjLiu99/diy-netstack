#ifndef NTIMER_H
#define NTIMER_H

#include "net_err.h"
#include "nlist.h"
#include "net_cfg.h"


#define NET_TIMER_RELOAD  (1 << 0)
#define NET_TIMER_ADDED   (1 << 1)
#define TIMER_NAME_SIZE 32


struct _net_timer_t;
typedef void (*timer_proc_t) (struct _net_timer_t *timer, void *arg);   //定时回调函数
typedef struct _net_timer_t {
    char name[TIMER_NAME_SIZE];
    int flags;

    int curr;
    int reload;
    timer_proc_t proc;
    void *arg;

    nlist_node_t node;

}net_timer_t;

net_err_t net_timer_init(void);

//添加定时器
net_err_t net_timer_add (net_timer_t *timer, const char *name, timer_proc_t proc, void *arg, int ms, int flags);

//删除定时器
void net_timer_remove (net_timer_t *timer);

//扫描定时器列表,传入两次扫描的时间间隔
net_err_t net_timer_check_tmo (int diff_ms);


int net_timer_first_tmo (void);
#endif