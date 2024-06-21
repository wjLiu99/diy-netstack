#include "udp.h"
#include "dbg.h"
#include "mblock.h"
#include "nlist.h"
#include "net_cfg.h"
#include "ipv4.h"
#include "socket.h"
#include "sock.h"

static udp_t udp_tbl[UDP_MAX_NR];
static mblock_t udp_mblock;
static nlist_t udp_list;

net_err_t udp_init (void) {
    dbg_info(DBG_UDP, "udp init..");
    mblock_init(&udp_mblock, udp_tbl, sizeof(udp_t), UDP_MAX_NR, NLOCKER_NONE);
    nlist_init(&udp_list);
    dbg_info(DBG_UDP, "udp init done");
    return NET_ERR_OK;

}


static net_err_t udp_sendto (struct _sock_t *s, const void *buf, size_t len, int flags, 
    const struct x_sockaddr *dest, x_socklen_t dest_len, ssize_t *result_len ) {
    
  

    ipaddr_t dest_ip;
    struct x_sockaddr_in *addr = (struct x_sockaddr_in *)dest;
    ipaddr_from_buf(&dest_ip, addr->sin_addr.addr_array);
    uint16_t dport = x_ntohs(addr->sin_port);
    //如果sock绑定了地址，发送地址必须相同
    if (!ipaddr_is_any(&s->remote_ip) && !ipaddr_is_equal(&dest_ip, &s->remote_ip)) {
        dbg_error(DBG_UDP, "dest is incorrect");
        return NET_ERR_PARAM;
    }

    if (s->remote_port && (s->remote_port != dport)) {
        dbg_error(DBG_UDP, "dest is incorrect");
        return NET_ERR_PARAM;
    }
    pktbuf_t *pktbuf = pktbuf_alloc((int)len);
    if (!pktbuf) {
        dbg_error(DBG_UDP, "no pktbuf");
        return NET_ERR_MEM;
    }
    pktbuf_reset_acc(pktbuf);
    net_err_t err = pktbuf_write(pktbuf, (uint8_t *)buf, (int)len);
    if (err < 0) {
        dbg_error(DBG_UDP, "copy_data err");
        goto end;
    }
    //不能直接传网卡默认地址
    // err = ipv4_out(s->protocol, &dest_ip, &netif_get_default()->ipaddr, pktbuf);
    err = ipv4_out(s->protocol, &dest_ip, &s->remote_ip, pktbuf);

    if (err < 0) {
        dbg_error(DBG_UDP, "send err");
        goto end;
    }
    *result_len = (ssize_t)len;
    return NET_ERR_OK;
end:
    pktbuf_free(pktbuf);
    return err;
}

sock_t *udp_create (int family, int protocol) {

    static const sock_ops_t udp_ops = {
        .setopt = sock_setopt,
        .sendto = udp_sendto,

    };
    udp_t *udp = mblock_alloc(&udp_mblock, -1);
    if (!udp) {
        dbg_error(DBG_UDP, "udp alloc err");
        return (sock_t *)0;
    }

    net_err_t err = sock_init(&udp->base, family, protocol, &udp_ops);
    if (err < 0) {
        dbg_error(DBG_UDP, "create udp failed");
        mblock_free(&udp_mblock, udp);
        return (sock_t *)0;
    }

    nlist_init(&udp->recv_list);
    udp->base.recv_wait = &udp->recv_wait;
    
    if (sock_wait_init(udp->base.recv_wait) < 0) {
        dbg_error(DBG_UDP, "init recv wait err");
        goto create_failed;
    }


    nlist_insert_last(&udp_list, &udp->base.node);


    return (sock_t *)udp;

create_failed:
    sock_uninit(&udp->base);
    return (sock_t *)0;
}