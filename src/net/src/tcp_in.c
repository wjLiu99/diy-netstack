#include "tcp_in.h"
#include "ntools.h"
#include "protocol.h"
#include "tcp_out.h"

void tcp_set_init(tcp_seg_t *seg, pktbuf_t *buf, ipaddr_t *local, ipaddr_t *remote) {
    seg->buf = buf;
    seg->hdr = (tcp_hdr_t *)pktbuf_data(buf);
    ipaddr_copy(&seg->local_ip, local);
    ipaddr_copy(&seg->remote_ip, remote);
    seg->data_len = buf->total_size - tcp_hdr_size(seg->hdr);
    seg->seq = seg->hdr->seq;
    //syn和fin也占一位长度
    seg->seq_len = seg->data_len + seg->hdr->f_syn + seg->hdr->f_fin;
}

net_err_t tcp_in (pktbuf_t *buf, ipaddr_t *src, ipaddr_t *dest) {
    tcp_hdr_t *tcp_hdr = (tcp_hdr_t *)pktbuf_data(buf);

    if (tcp_hdr->checksum) {
        pktbuf_reset_acc(buf);
        if (checksum_peso(buf,  dest, src, NET_PROTOCOL_TCP)) {
            dbg_warning(DBG_TCP, "tcp checksum err");
            return NET_ERR_CHECKSUM;
        }
    }

    if ((buf->total_size < sizeof(tcp_hdr_t)) || (buf->total_size < tcp_hdr_size(tcp_hdr))) {
        dbg_error(DBG_TCP, "tcppkt size err");
        return NET_ERR_SIZE;
    }

    if (!tcp_hdr->sport || !tcp_hdr->dport) {
        dbg_warning(DBG_TCP, "port err");
        return NET_ERR_NONE;
    }

    if(tcp_hdr->flags == 0) {
        dbg_warning(DBG_TCP, "flag err");
        return NET_ERR_NONE;
    }

    tcp_hdr->dport = x_ntohs(tcp_hdr->dport);
    tcp_hdr->sport = x_ntohs(tcp_hdr->sport);
    tcp_hdr->seq = x_ntohl(tcp_hdr->seq);
    tcp_hdr->ack = x_ntohl(tcp_hdr->ack);
    tcp_hdr->urgptr = x_ntohs(tcp_hdr->urgptr);
    tcp_hdr->win = x_ntohs(tcp_hdr->win);
    tcp_seg_t seg;
    tcp_set_init(&seg, buf, dest, src);

    tcp_send_reset(&seg);

    return NET_ERR_OK;


}