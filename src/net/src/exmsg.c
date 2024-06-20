#include "exmsg.h"
#include "sys.h"
#include "fixmq.h"
#include "dbg.h"
#include "mblock.h"
#include "ntimer.h"
#include "ipv4.h"

static void *msg_tbl[EXMSG_MSG_CNT];
static fixmq_t mq;


static exmsg_t msg_buffer[EXMSG_MSG_CNT];
static mblock_t msg_mblock;

static net_err_t do_netif_in (exmsg_t *msg) {
    netif_t *netif = msg->netif.netif;
    pktbuf_t *buf;
    //一次性取出所有数据包
    while ((buf = netif_get_in(netif, -1))){
        dbg_info(DBG_EXMSG, "recv a packet");

        if (netif->linker_layer) {
            net_err_t err = netif->linker_layer->in(netif, buf);
            if (err < 0) {
                pktbuf_free(buf);
                dbg_warning(DBG_EXMSG, "netif in failed, err = %d",err);
            }
        } else {
            //环回接口没有链路层协议，直接交给ip层处理
            net_err_t err = ipv4_in(netif, buf);
            if (err < 0) {
                pktbuf_free(buf);
                dbg_warning(DBG_EXMSG, "netif in failed, err = %d",err);
            }
        }

    }

    return NET_ERR_OK;
}


static net_err_t do_func (func_msg_t *func_msg) {
    dbg_info(DBG_EXMSG, "call func");
    func_msg->err = func_msg->func(func_msg);
    //执行完毕通知应用程序
    sys_sem_notify(func_msg->wait_sem);
    return NET_ERR_OK;
}
static void work_thread(void * arg){
    dbg_info(DBG_EXMSG,"exmsg working....\n");
    net_time_t time;
    sys_time_curr(&time);
    while(1){
        //定时器列表第一个超时时间
        int ms = net_timer_first_tmo();
        //阻塞等，工作线程就是一直取消息处理
        exmsg_t *msg = (exmsg_t *)fixmq_recv(&mq, ms);

        if (msg) {

            dbg_info(DBG_EXMSG, "recv a msg %p: %d\n", msg, msg->type);
        
            switch (msg->type)
            {
            case NET_EXMSG_NETIF_IN:
                do_netif_in(msg);
                break;

            case NET_EXMSG_FUNC:
                do_func(msg->func);
                break;
            
            default:
                break;
            }
            mblock_free(&msg_mblock, msg);

        } 
        int diff_ms = sys_time_goes(&time);

        net_timer_check_tmo(diff_ms);
        
  
        
    }
}

net_err_t exmsg_init(void){
    dbg_info(DBG_EXMSG, "exmsg init");
    net_err_t err = fixmq_init(&mq, msg_tbl, EXMSG_MSG_CNT, EXMSG_LOCKER);
    if(err < 0){
        dbg_error(DBG_EXMSG, "fixmq init failed");
        return err;
    }
    err = mblock_init(&msg_mblock, msg_buffer, sizeof(exmsg_t), EXMSG_MSG_CNT, EXMSG_LOCKER);
    if(err < 0){
        dbg_error(DBG_EXMSG, "mblock init failed");
        return err;
    }
    dbg_info(DBG_EXMSG, "exmsg init done");
    

    return NET_ERR_OK;
}

net_err_t exmsg_start(void){
    sys_thread_t thread = sys_thread_create(work_thread, (void *)0);
    if(thread == SYS_THREAD_INVALID){
        return NET_ERR_SYS;
    }
    return NET_ERR_OK;
}

net_err_t exmsg_netif_in(netif_t *netif){
    //没有内存块不应该等，之后用中断处理程序调用该函数不应该阻塞，造成数据包的丢失也是正常现象，网络本来就不稳定
    exmsg_t *msg = mblock_alloc(&msg_mblock, -1);
    if(!msg){
        dbg_warning(DBG_EXMSG, "no free msg");
        return NET_ERR_MEM;
    }

    msg->type = NET_EXMSG_NETIF_IN;
    msg->netif.netif = netif;
    net_err_t err = fixmq_send(&mq, msg, -1);
    if(err < 0){
        dbg_warning(DBG_EXMSG, "mq full");
        mblock_free(&msg_mblock, msg);
        return err;
    }

    return NET_ERR_OK;
}

net_err_t exmsg_func_exec (exmsg_func_t func, void *param) {
    func_msg_t func_msg;
    func_msg.func = func;
    func_msg.param = param;
    func_msg.err = NET_ERR_OK;
    func_msg.thread = sys_thread_self();
    func_msg.wait_sem = sys_sem_create(0);
    if (func_msg.wait_sem == SYS_SEM_INVALID) {
        dbg_error(DBG_EXMSG, "err create wait sem");
        return NET_ERR_MEM;
    }

    //此时是应用程序调用，应该阻塞等
    exmsg_t *msg = mblock_alloc(&msg_mblock, 0);
    if(!msg){
        dbg_warning(DBG_EXMSG, "no free msg");
        sys_sem_free(func_msg.wait_sem);
        return NET_ERR_MEM;
    }

    //填充消息体，func可以不设置为指针，先分配完消息再填充funcmsg里面的内容
    msg->type = NET_EXMSG_FUNC;
    msg->func = &func_msg;
    net_err_t err = fixmq_send(&mq, msg, 0);
    if(err < 0){
        dbg_warning(DBG_EXMSG, "mq full");
        sys_sem_free(func_msg.wait_sem);
        mblock_free(&msg_mblock, msg);
        return err;
    }

    //等待消息处理完毕才返回， 如果不等的话直接返回局部变量func_msg会被释放，影响工作线程处理消息，会发生越界异常
    sys_sem_wait(func_msg.wait_sem, 0);

    return func_msg.err;


}