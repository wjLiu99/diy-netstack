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
//获取发送信息
static void get_send_info (tcp_t *tcp, int *doff, int *dlen) {
    //偏移量为下一个要发送的字节在当前发送缓冲区的位置
    *doff = tcp->send.nxt - tcp->send.una;
    //发送数据长度为发送缓冲区大小减去待发送字节位置，就是发送缓冲区内待发送字节后面所有的数据
    *dlen = tcp_buf_count(&tcp->send.buf) - *doff;
    
    *dlen = (*dlen > tcp->mss) ? tcp->mss : *dlen;

}

static int copy_send_data (tcp_t *tcp, pktbuf_t *buf, int doff, int dlen) {
    if (dlen == 0) {
        return 0;

    }

    net_err_t err = pktbuf_resize(buf, (int)buf->total_size + dlen);
    if (err < 0){
        dbg_error(DBG_TCP, "pkfbuf resize err");
        return -1;
    }
    int hdr_size = tcp_hdr_size((tcp_hdr_t *)pktbuf_data(buf));
    pktbuf_reset_acc(buf);
    pktbuf_seek(buf, hdr_size);
    //读发送缓冲区，拷贝到pktbuf中
    tcp_buf_read_send(&tcp->send.buf, doff, buf, dlen);
    return dlen;


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

    //发送数据的长度和偏移量
    int dlen, doff;
    //获取发送相关信息
    get_send_info(tcp, &doff, &dlen);
    if (dlen < 0) {
        //等于0可以发送，因为syn和fin报文数据量都是0
        return NET_ERR_OK;
    }
    //将tcp缓冲区数据拷贝到pktbuf中
    copy_send_data(tcp, buf, doff, dlen);


    //syn和fin也占一个编号,下一个待发送的数据
    tcp->send.nxt += out->f_syn + out->f_fin + dlen;
    net_err_t err = send_out(out, buf, &tcp->base.remote_ip, &tcp->base.local_ip);

    if (err < 0) {
        dbg_error(DBG_TCP, "transmit err");
        pktbuf_free(buf);
        return err;
    }
    

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
    //对方确认收到的字节量
    int ack_cnt = hdr->ack - tcp->send.una;
    //发送缓冲区中已发送未确认的字节量
    int unacked = tcp->send.nxt - tcp->send.una;
    int cur_acked = (ack_cnt > unacked) ? unacked : ack_cnt;

    if (cur_acked > 0) { //有数据被对方确认接收
        tcp->send.una += cur_acked;
        //如果对方对fin报文进行了确认，减完值会为1
        cur_acked -= tcp_buf_remove(&tcp->send.buf, cur_acked);
        //唤醒等待写的进程
        sock_wakeup(&tcp->base, SOCK_WAIT_WRITE, NET_ERR_OK);
            //fin报文已发出，且对方确认接收清空该标志位
        if (tcp->flags.fin_out && cur_acked) {
            tcp->flags.fin_out = 0;
        }

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


int tcp_write_sendbuf (tcp_t *tcp, const uint8_t *data, int len) {
    int free_cnt = tcp_buf_free_cnt(&tcp->send.buf);
    if (free_cnt <= 0) {
        return 0;
    }
    int wr_len = (len > free_cnt) ? free_cnt : len;
    tcp_buf_write_send(&tcp->send.buf, data, wr_len);
    return wr_len;
}