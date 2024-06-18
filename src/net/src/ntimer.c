#include "ntimer.h"
#include "dbg.h"
#include "net_cfg.h"
#include "sys.h"



static nlist_t timer_list;

#if DBG_DISPLAY_ENABLED(DBG_NTIMER)

static void display_timer_list (void) {
    plat_printf("-------timer list-----------\n");
    nlist_node_t *node;
    int index = 0;
    nlist_for_each(node, &timer_list) {
        net_timer_t *timer = nlist_entry(node, net_timer_t, node);
        plat_printf("%d: %s, period = %d, curr: %dms, reload: %dms \n", index++, timer->name, timer->flags & NET_TIMER_RELOAD ? 1 : 0
         ,timer->curr, timer->reload);
    }

    plat_printf("-----------timer list end--------------\n");
}
#else
#define display_timer_list()
#endif
net_err_t net_timer_init (void) {
    dbg_info(DBG_NTIMER, "timer init ... ");


    nlist_init(&timer_list);
    dbg_info(DBG_NTIMER, "timer init done ...");
    return NET_ERR_OK;
}

static void insert_timer (net_timer_t *timer) {
    nlist_node_t *node;

    nlist_for_each(node, &timer_list) {
        net_timer_t *cur = nlist_entry(node, net_timer_t, node);
        if (timer->curr > cur->curr) {
            timer->curr -= cur->curr;
        } else if (timer->curr == cur->curr) {
            timer->curr = 0;
            nlist_insert_after(&timer_list, &cur->node, &timer->node);
            return ;
        } else {
            cur->curr -= timer->curr;
            nlist_node_t *pre = nlist_node_pre(&cur->node);
            if (pre) {
                nlist_insert_after(&timer_list, pre, &timer->node);
            } else {
                nlist_insert_first(&timer_list, &timer->node);
            }

            return;
        }

    }
    nlist_insert_last(&timer_list, &timer->node);
}
net_err_t net_timer_add (net_timer_t *timer, const char *name, timer_proc_t proc, void *arg, int ms, int flags){

    dbg_info(DBG_NTIMER, "insert timer: %s", name);
    plat_strncpy(timer->name, name, TIMER_NAME_SIZE);
    timer->name[TIMER_NAME_SIZE - 1] =  '\0';
    timer->reload = ms;
    timer->curr = ms;
    timer->proc = proc;
    timer->arg = arg;
    timer->flags = flags;

    insert_timer(timer);
    timer->flags |= NET_TIMER_ADDED;
    display_timer_list();

    return NET_ERR_OK;
}


void net_timer_remove (net_timer_t *timer) {
    dbg_info(DBG_NTIMER, "remove timer..");

    if (timer->flags & NET_TIMER_ADDED){
        nlist_node_t *next = nlist_node_next(&timer->node);
        if (next) {
            net_timer_t *ntimer = nlist_entry(next, net_timer_t, node);
            ntimer->curr += timer->curr;
         }

        nlist_remove(&timer_list, &timer->node);
        display_timer_list();
        return;
    }

    dbg_info(DBG_NTIMER, "timer not add");

}

net_err_t net_timer_check_tmo (int diff_ms) {
    nlist_t wait_list; //延时处理超时事件
    nlist_init(&wait_list);
    nlist_node_t *node = nlist_first(&timer_list);

    while (node) {
        net_timer_t *timer = nlist_entry(node, net_timer_t, node);
        if (timer->curr > diff_ms) {
            timer->curr -= diff_ms;
            break;
        }
        nlist_node_t *next = nlist_node_next(&timer->node);

        diff_ms -= timer->curr;
        timer->curr = 0;

        nlist_remove(&timer_list, &timer->node);
        nlist_insert_last(&wait_list, &timer->node);
        node = next;
    }


    while((node = nlist_remove_first(&wait_list)) != (nlist_node_t *)0) {
        net_timer_t *timer = nlist_entry(node, net_timer_t, node);
        timer->proc(timer, timer->arg);

        if (timer->flags & NET_TIMER_RELOAD) {
            timer->curr = timer->reload;
            insert_timer(timer);
        }
    }

    display_timer_list();
    return NET_ERR_OK;
}

int net_timer_first_tmo (void) {
    nlist_node_t *node = nlist_first(&timer_list);
    if (node) {
        net_timer_t *timer = nlist_entry(node, net_timer_t, node);
        return timer->curr;
    }

    return 0;
}