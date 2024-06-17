#include "netif.h"
#include "mblock.h"


static netif_t netif_buffer[NETIF_DEV_CNT];
static mblock_t netif_mblock;
static nlist_t netif_list; //分配之后的网络接口链表
static netif_t *netif_default;

net_err_t netif_init (void) {
    dbg_info(DBG_NETIF, "init netif..");
    
    nlist_init(&netif_list);
    mblock_init(&netif_mblock, netif_buffer, sizeof(netif_t), NETIF_DEV_CNT, NLOCKER_NONE); //只有工作线程访问这块代码

    netif_default = (netif_t *)0;

    dbg_info(DBG_NETIF, "netif init done ...");
    return NET_ERR_OK;
}

netif_t *netif_open(const char *dev_name){
    //不能等，工作线程不能卡住
    netif_t *netif = (netif_t *)mblock_alloc(&netif_mblock, -1);
    if (!netif) {
        dbg_error(DBG_NETIF, "no netif");
        return (netif_t *)0;
    }

    ipaddr_set_any(&netif->gateway);
    ipaddr_set_any(&netif->ipaddr);
    ipaddr_set_any(&netif->netmask);

    plat_strncpy(netif->name, dev_name, NETIF_NAME_SIZE);
    netif->name[NETIF_NAME_SIZE - 1] = '\0';

    plat_memset(&netif->hwaddr, 0, sizeof(netif_hwaddr_t));
    netif->type = NETIF_TYPE_NONE;
    netif->mtu = 0;

    nlist_node_init(&netif->node);

    //加锁，工作线程，输入输出线程都会操作该队列
    net_err_t err = fixmq_init(&netif->in_mq, netif->in_mq_buf, NETIF_INMQ_SIZE, NLOCKER_THREAD);
    if (err < 0) {
        dbg_error(DBG_NETIF, "init inmq err");
        return (netif_t *)0;
    }
    err = fixmq_init(&netif->out_mq, netif->out_mq_buf, NETIF_OUTMQ_SIZE, NLOCKER_THREAD);
    if (err < 0) {
        dbg_error(DBG_NETIF, "init outmq err");
        fixmq_destory(&netif->in_mq);
        return (netif_t *)0;
    }

    netif->state = NETIF_OPENED;
    //初始化完成，加入网卡队列
    nlist_insert_last(&netif_list, &netif->node);
    return netif;

}