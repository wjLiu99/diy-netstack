#include "exmsg.h"
#include "sys.h"
#include "fixmq.h"
#include "dbg.h"
#include "mblock.h"



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
        pktbuf_fill(buf, 0x11, 6);
        net_err_t err = netif_out(netif, (ipaddr_t *)0, buf);
        if (err < 0) {
            pktbuf_free(buf);
        }

    }

    return NET_ERR_OK;
}

static void work_thread(void * arg){
    dbg_info(DBG_EXMSG,"exmsg working....\n");
    while(1){
        //阻塞等，工作线程就是一直取消息处理
        exmsg_t *msg = (exmsg_t *)fixmq_recv(&mq, 0);
        dbg_info(DBG_EXMSG, "recv a msg %p: %d\n", msg, msg->type);
        
        switch (msg->type)
        {
        case NET_EXMSG_NETIF_IN:
            do_netif_in(msg);
            break;
        
        default:
            break;
        }
        mblock_free(&msg_mblock, msg);
        
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

    return err;
}