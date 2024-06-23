#include "tcp_out.h"
#include "ntools.h"
#include "protocol.h"
#include "ipv4.h"


static net_err_t send_out(tcp_hdr_t *out, pktbuf_t *buf, ipaddr_t *dest, ipaddr_t *src) {
    out->dport = x_ntohs(out->dport);
    out->sport = x_ntohs(out->sport);
    out->seq = x_ntohl(out->seq);
    out->ack = x_ntohl(out->ack);
    out->urgptr = x_ntohs(out->urgptr);
    out->win = x_ntohs(out->win);
    out->checksum = 0;
    out->checksum = checksum_peso(buf, dest, src, NET_PROTOCOL_TCP);
    net_err_t err = ipv4_out(NET_PROTOCOL_TCP, dest, src, buf);
    if (err < 0) {
        dbg_error(DBG_TCP, "send tcp err");
        return NET_ERR_NONE;
    }
    return NET_ERR_OK;
}

net_err_t tcp_send_reset (tcp_seg_t *seg) {
    pktbuf_t *buf = pktbuf_alloc(sizeof(tcp_hdr_t));
    if (!buf) {
        dbg_error(DBG_TCP, "no pktbuf");
        return NET_ERR_MEM;

    }
    tcp_hdr_t *in = seg->hdr;
    //填充包头
    tcp_hdr_t *out = (tcp_hdr_t *)pktbuf_data(buf);
    out->sport = in->dport;
    out->dport = in->sport;
    out->flags = 0;
    out->f_rst = 1;
    tcp_set_hdr_size(out, sizeof(tcp_hdr_t));
    out->win = out->urgptr = 0;
    //如果对方发来的包确认序号有效，reset报文的序号为对方数据包的确认序号,确认序号无效，则发送对方包的序号加数据长度的确认包
    if (in->f_ack) {
        out->seq = in->ack;
        out->ack = 0;
        out->f_ack = 0;
    } else {
        out->ack = in->seq + seg->seq_len;
        out->f_ack = 1;
    }
    net_err_t err =  send_out(out, buf, &seg->remote_ip, &seg->local_ip);
    if (err < 0) {
        dbg_error(DBG_TCP, "send reset err");
        pktbuf_free(buf);
        return err;
    }

    return NET_ERR_OK;

}