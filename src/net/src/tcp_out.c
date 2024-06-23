#include "tcp_out.h"
#include "ntools.h"
#include "protocol.h"
#include "ipv4.h"


static net_err_t send_out(tcp_hdr_t *out, pktbuf_t *buf, ipaddr_t *dest, ipaddr_t *src) {
    tcp_display_pkt("tcp out", out, buf);
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

//通用数据发送接口
net_err_t tcp_transmit (tcp_t *tcp) {
   pktbuf_t *buf = pktbuf_alloc(sizeof(tcp_hdr_t));
    if (!buf) {
        dbg_error(DBG_TCP, "no pktbuf");
        return NET_ERR_MEM;

    }
    
    //填充包头
    tcp_hdr_t *out = (tcp_hdr_t *)pktbuf_data(buf);
    plat_memset(out, 0, sizeof(tcp_hdr_t));
    out->sport = tcp->base.local_port;
    out->dport = tcp->base.remote_port;
    //发送序号为发送的下一个字节，确认序号为期望接受的下一个字节
    out->seq = tcp->send.nxt;
    out->ack = tcp->recv.nxt;
    out->flags = 0;
    out->f_syn = tcp->flags.syn_out;
    out->f_ack = tcp->flags.irs_valid; //是否已经收到对方syn
    out->f_fin = tcp->flags.fin_out; // 是否已经收到对方的fin为，并且调用close函数发送fin报文
    out->win = 1024;
    out->urgptr = 0;
    tcp_set_hdr_size(out, sizeof(tcp_hdr_t));
    net_err_t err = send_out(out, buf, &tcp->base.remote_ip, &tcp->base.local_ip);

    if (err < 0) {
        dbg_error(DBG_TCP, "transmit err");
        pktbuf_free(buf);
        return err;
    }
    //syn和fin也占一个编号
    tcp->send.nxt += out->f_syn + out->f_fin;

    return NET_ERR_OK;
}

net_err_t tcp_send_syn (tcp_t *tcp) {
    tcp->flags.syn_out = 1;
    tcp_transmit(tcp);
    return NET_ERR_OK; 
}

net_err_t tcp_ack_process (tcp_t *tcp, tcp_seg_t *seg) {
    tcp_hdr_t *hdr = seg->hdr;
    //如果是发送了syn报文，对方发送的确认就需要对标志位处理，已发送未确认调整
    if (tcp->flags.syn_out) {
        tcp->send.una++;
        tcp->flags.syn_out = 0;
    }
    //fin报文已发出，且对方确认接收清空该标志位
    if (tcp->flags.fin_out && (hdr->ack - tcp->send.una > 0)) {
        tcp->flags.fin_out = 0;
    }

    return NET_ERR_OK;

}

net_err_t tcp_send_ack (tcp_t *tcp, tcp_seg_t *seg) {
    pktbuf_t *buf = pktbuf_alloc(sizeof(tcp_hdr_t));
    if (!buf) {
        dbg_error(DBG_TCP, "no buf");
        return NET_ERR_MEM;
    }


    tcp_hdr_t *out = (tcp_hdr_t *)pktbuf_data(buf);
    plat_memset(out, 0, sizeof(tcp_hdr_t));
    out->sport = tcp->base.local_port;
    out->dport = tcp->base.remote_port;
    out->seq = tcp->send.nxt;
    out->ack = tcp->recv.nxt;
    out->flags = 0;
    out->f_syn = tcp->flags.syn_out;
    out->f_ack = 1; //是否已经收到对方syn
    out->win = 1024;
    out->urgptr = 0;
    tcp_set_hdr_size(out, sizeof(tcp_hdr_t));
    net_err_t err = send_out(out, buf, &tcp->base.remote_ip, &tcp->base.local_ip);

    if (err < 0) {
        dbg_error(DBG_TCP, "send ack err");
        pktbuf_free(buf);
        return err;
    }

    return NET_ERR_OK;

}

net_err_t tcp_send_fin (tcp_t *tcp) {
    tcp->flags.fin_out = 1;
    tcp_transmit(tcp);
    return NET_ERR_OK;

}