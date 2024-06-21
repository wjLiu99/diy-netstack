#include "raw.h"
#include "dbg.h"
#include "mblock.h"
#include "nlist.h"
#include "net_cfg.h"
#include "ipv4.h"
#include "socket.h"

static raw_t raw_tbl[RAW_MAX_NR];
static mblock_t raw_mblock;
static nlist_t raw_list;


net_err_t raw_init(void) {
    dbg_info(DBG_RAW, "raw init..");
    mblock_init(&raw_mblock, raw_tbl, sizeof(raw_t), RAW_MAX_NR, NLOCKER_NONE);
    nlist_init(&raw_list);
    dbg_info(DBG_RAW, "raw init done");
    return NET_ERR_OK;

}

static net_err_t raw_sendto (struct _sock_t *s, const void *buf, size_t len, int flags, 
    const struct x_sockaddr *dest, x_socklen_t dest_len, ssize_t *result_len ) {
    
  

    ipaddr_t dest_ip;
    struct x_sockaddr_in *addr = (struct x_sockaddr_in *)dest;
    ipaddr_from_buf(&dest_ip, addr->sin_addr.addr_array);
    //如果sock绑定了地址，发送地址必须相同
    if (!ipaddr_is_any(&s->remote_ip) && !ipaddr_is_equal(&dest_ip, &s->remote_ip)) {
        dbg_error(DBG_RAW, "dest is incorrect");
        return NET_ERR_PARAM;
    }

    pktbuf_t *pktbuf = pktbuf_alloc((int)len);
    if (!pktbuf) {
        dbg_error(DBG_RAW, "no pktbuf");
        return NET_ERR_MEM;
    }
    pktbuf_reset_acc(pktbuf);
    net_err_t err = pktbuf_write(pktbuf, (uint8_t *)buf, (int)len);
    if (err < 0) {
        dbg_error(DBG_RAW, "copy_data err");
        goto end;
    }

    err = ipv4_out(s->protocol, &dest_ip, &netif_get_default()->ipaddr, pktbuf);
    if (err < 0) {
        dbg_error(DBG_RAW, "send err");
        goto end;
    }
    *result_len = (ssize_t)len;
    return NET_ERR_OK;
end:
    pktbuf_free(pktbuf);
    return err;
}


static net_err_t raw_recvfrom (struct _sock_t *s,  void *buf, size_t len, int flags, 
     struct x_sockaddr *dest, x_socklen_t *dest_len, ssize_t *result_len ){
        return NET_ERR_WAIT;
}



sock_t *raw_create (int family, int protocol) {

    static const sock_ops_t raw_ops = {
        .sendto = raw_sendto,
        .recvfrom = raw_recvfrom,
        .setopt = sock_setopt,

    };
    raw_t *raw = mblock_alloc(&raw_mblock, -1);
    if (!raw) {
        dbg_error(DBG_RAW, "raw alloc err");
        return (sock_t *)0;
    }

    net_err_t err = sock_init(&raw->base, family, protocol, &raw_ops);
    if (err < 0) {
        dbg_error(DBG_RAW, "create raw failed");
        mblock_free(&raw_mblock, raw);
        return (sock_t *)0;
    }


    raw->base.recv_wait = &raw->recv_wait;
    if (sock_wait_init(raw->base.recv_wait) < 0) {
        dbg_error(DBG_RAW, "init recv wait err");
        goto create_failed;
    }


    nlist_insert_last(&raw_list, &raw->base.node);

    return (sock_t *)raw;

create_failed:
    sock_uninit(&raw->base);
    return (sock_t *)0;
}