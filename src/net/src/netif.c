#include "netif.h"
#include "mblock.h"
#include "pktbuf.h"
#include "exmsg.h"
#include "protocol.h"
#include "ether.h"
#include "ipaddr.h"
#include "ipv4.h"

static netif_t netif_buffer[NETIF_DEV_CNT];
static mblock_t netif_mblock;
static nlist_t netif_list; //分配之后的网络接口链表
static netif_t *netif_default;

static const link_layer_t *link_layer[NETIF_TYPE_SIZE];

#if DBG_DISPLAY_ENABLED(DBG_NETIF)
void display_netif_list (void) {
    nlist_node_t * node;

    plat_printf("netif list:\n");
    nlist_for_each(node, &netif_list) {
        netif_t * netif = nlist_entry(node, netif_t, node);
        plat_printf("%s:", netif->name);
        switch (netif->state) {
            case NETIF_CLOSED:
                plat_printf(" %s ", "closed");
                break;
            case NETIF_OPENED:
                plat_printf(" %s ", "opened");
                break;
            case NETIF_ACTIVE:
                plat_printf(" %s ", "active");
                break;
            default:
                break;
        }
        switch (netif->type) {
            case NETIF_TYPE_ETHER:
                plat_printf(" %s ", "ether");
                break;
            case NETIF_TYPE_LOOP:
                plat_printf(" %s ", "loop");
                break;
            default:
                break;
        }
        plat_printf(" mtu=%d ", netif->mtu);
        plat_printf("\n");
        dbg_dump_hwaddr("mac:", netif->hwaddr.addr, netif->hwaddr.len);
        dbg_dump_ip(" ip:", &netif->ipaddr);
        dbg_dump_ip(" netmask:", &netif->netmask);
        dbg_dump_ip(" gateway:", &netif->gateway);

        // 队列中包数量的显示
        plat_printf("\n");
    }
}
#else
#define display_netif_list()
#endif


net_err_t netif_init (void) {
    dbg_info(DBG_NETIF, "init netif..");
    
    nlist_init(&netif_list);
    mblock_init(&netif_mblock, netif_buffer, sizeof(netif_t), NETIF_DEV_CNT, NLOCKER_NONE); //只有工作线程访问这块代码

    netif_default = (netif_t *)0;
    plat_memset((void *)link_layer, 0, sizeof(link_layer));

    dbg_info(DBG_NETIF, "netif init done ...");
    return NET_ERR_OK;
}

net_err_t netif_register_layer (int type, const link_layer_t *layer){
    if ((type < 0) || (type >= NETIF_TYPE_SIZE)){
        dbg_error(DBG_NETIF, "type err");
        return NET_ERR_PARAM;
    }

    if (link_layer[type]) {
        dbg_error(DBG_NETIF, "layer type exist");
        return NET_ERR_EXIST;
    }
    link_layer[type] = layer;
    return NET_ERR_OK;

}

static const link_layer_t *netif_get_layer (int type) {
      if ((type < 0) || (type >= NETIF_TYPE_SIZE)){
        dbg_error(DBG_NETIF, "type err");
        return (const link_layer_t *)0;
    }

    return link_layer[type];
}

netif_t *netif_open(const char *dev_name, const netif_ops_t *ops, void *ops_data){
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
        mblock_free(&netif_mblock, netif);
        return (netif_t *)0;
    }
    err = fixmq_init(&netif->out_mq, netif->out_mq_buf, NETIF_OUTMQ_SIZE, NLOCKER_THREAD);
    if (err < 0) {
        dbg_error(DBG_NETIF, "init outmq err");
        fixmq_destory(&netif->in_mq);
        mblock_free(&netif_mblock, netif);
        return (netif_t *)0;
    }
    //驱动应该在内部可以改写ops_data
    netif->ops = ops;
    netif->ops_data = ops_data;
    //针对特定网卡驱动进行打开设置
    err = ops->open(netif, ops_data);
    if (err < 0) {
        dbg_error(DBG_NETIF, "netif hwopen err");
        goto free_src;
    }
    netif->state = NETIF_OPENED;

    if (netif->type == NETIF_TYPE_NONE) {
        dbg_error(DBG_NETIF, "netif type err");
        goto free_src;
    }


    netif->linker_layer = netif_get_layer(netif->type);
    if (!netif->linker_layer && (netif->type != NETIF_TYPE_LOOP)) {
        dbg_error(DBG_NETIF, "no link layer");
        goto free_src;
    }
    
    //初始化完成，加入网卡队列
    nlist_insert_last(&netif_list, &netif->node);
    display_netif_list();
    return netif;

free_src:
    if (netif->state == NETIF_OPENED) {
        ops->close(netif);
    }

    fixmq_destory(&netif->in_mq);
    fixmq_destory(&netif->out_mq);
    mblock_free(&netif_mblock, netif);


    return (netif_t *)0;
}


net_err_t netif_set_addr (netif_t *netif, ipaddr_t *ip, ipaddr_t *netmask, ipaddr_t *gateway){
    ipaddr_copy(&netif->ipaddr, ip ? ip : ipaddr_get_any());
    ipaddr_copy(&netif->netmask, netmask ? netmask : ipaddr_get_any());
    ipaddr_copy(&netif->gateway, gateway ? gateway : ipaddr_get_any());
    return NET_ERR_OK;
}
net_err_t netif_set_hwaddr (netif_t *netif, const char *hwaddr, int len) {
    plat_memcpy (netif->hwaddr.addr, hwaddr, len);
    netif->hwaddr.len = len;
    return NET_ERR_OK;
}

//激活网卡
net_err_t netif_set_active (netif_t *netif) {
    if (netif->state != NETIF_OPENED) {
        dbg_error(DBG_NETIF, "netif not opened");
        return NET_ERR_STATE;
    }

    if (netif->linker_layer) {
        net_err_t err = netif->linker_layer->open(netif);
        if (err < 0) {
            dbg_error(DBG_NETIF, "link layer open failed");
            return err;
        }
    }
    ipaddr_t ip = ipaddr_get_netid(&netif->ipaddr, &netif->netmask);
    //激活网卡时添加路由表项
    rt_add(&ip, &netif->netmask, ipaddr_get_any(), netif);
    ipaddr_from_str(&ip, "255.255.255.255");
    rt_add(&netif->ipaddr, &ip, ipaddr_get_any(), netif);
    netif->state = NETIF_ACTIVE;

    if (!netif_default && (netif->type != NETIF_TYPE_LOOP)) {
        netif_set_default(netif);
    }

    display_netif_list();
    return NET_ERR_OK;
}
net_err_t netif_set_deactive (netif_t *netif){
    if (netif->state != NETIF_ACTIVE) {
        dbg_error(DBG_NETIF, "netif not active");
        return NET_ERR_STATE;
    }
    pktbuf_t *buf;
    while ((buf = fixmq_recv(&netif->in_mq, -1)) != (pktbuf_t *)0) {
        pktbuf_free(buf);
    }
        while ((buf = fixmq_recv(&netif->out_mq, -1)) != (pktbuf_t *)0) {
        pktbuf_free(buf);
    }

    if(netif->linker_layer) {
        netif->linker_layer->close(netif);
    }

    if (netif == netif_default) {
        rt_remove(ipaddr_get_any(), ipaddr_get_any());
        netif_default = (netif_t *)0;
    }

    ipaddr_t ip = ipaddr_get_netid(&netif->ipaddr, &netif->netmask);
    //删除路由表项
    rt_remove(&ip, &netif->netmask);
    ipaddr_from_str(&ip, "255.255.255.255");
    rt_remove(&netif->ipaddr, &ip);
    netif->state = NETIF_OPENED;
    display_netif_list();
    return NET_ERR_OK;
}


net_err_t netif_close (netif_t *netif){
    if (netif->state == NETIF_ACTIVE){
        dbg_error(DBG_NETIF, "netif is active");
        return NET_ERR_STATE;
    }
    netif->ops->close(netif);
    netif->state = NETIF_CLOSED;

    nlist_remove(&netif_list, &netif->node);
    mblock_free(&netif_mblock, netif);
    display_netif_list();
    return NET_ERR_OK;
}

//添加缺省网络接口
void netif_set_default (netif_t *netif){
    netif_default = netif;
    if (!ipaddr_is_any(&netif->gateway)) {
        if (netif_default) {
            rt_remove(ipaddr_get_any(), ipaddr_get_any());
        }
        rt_add(ipaddr_get_any(), ipaddr_get_any(), &netif->gateway, netif);
    }
    
}
netif_t *netif_get_default(void) {
    return netif_default;
}

net_err_t netif_put_in (netif_t *netif, pktbuf_t *buf, int tmo) {
    net_err_t err =fixmq_send(&netif->in_mq, (void *)buf, tmo);
    if (err < 0) {
        dbg_warning(DBG_NETIF, "netif inmq full");
        return NET_ERR_FULL;
    }
    //通知有数据到达
    exmsg_netif_in(netif);
    return NET_ERR_OK;
}

pktbuf_t *netif_get_in (netif_t *netif, int tmo) {
    pktbuf_t *buf = fixmq_recv(&netif->in_mq, tmo);
    if (buf) {
        pktbuf_reset_acc(buf);
        return buf;
    }
    dbg_info(DBG_NETIF, "netif inmq is empty");
    return (pktbuf_t *)0;
}


net_err_t netif_put_out (netif_t *netif, pktbuf_t *buf, int tmo) {
    net_err_t err =fixmq_send(&netif->out_mq, (void *)buf, tmo);
    if (err < 0) {
        dbg_warning(DBG_NETIF, "netif outmq full");
        return NET_ERR_FULL;
    }

    return NET_ERR_OK;
}
pktbuf_t *netif_get_out (netif_t *netif, int tmo){
     pktbuf_t *buf = fixmq_recv(&netif->out_mq, tmo);
    if (buf) {
        pktbuf_reset_acc(buf);
        return buf;
    }
    dbg_info(DBG_NETIF, "netif outmq is empty");
    return (pktbuf_t *)0;
}


net_err_t netif_out (netif_t *netif, ipaddr_t *ipaddr, pktbuf_t *buf) {
    if (netif->linker_layer) {
        net_err_t err = netif->linker_layer->out(netif, ipaddr, buf);
        if (err < 0) {
            dbg_warning(DBG_NETIF, "netif link out err");
            return err;
        }
    } else {
        //加入输出队列直接启动硬件发送
        net_err_t err = netif_put_out (netif, buf, -1);
        if (err < 0) {
            dbg_info(DBG_NETIF, "send failed, queue full");
            return NET_ERR_FULL;
        }

        return netif->ops->xmit(netif);

    }
    return NET_ERR_OK;
    
}

