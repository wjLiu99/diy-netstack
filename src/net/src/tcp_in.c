#include "tcp_in.h"
#include "ntools.h"
#include "protocol.h"
#include "tcp_out.h"
#include "tcp_stat.h"
#include "tcp_buf.h"

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
//检测seq是否合法
static int tcp_seq_acceptable (tcp_t *tcp, tcp_seg_t *seg) {
    uint32_t rcv_win = tcp_recv_window(tcp);
    if (seg->seq_len == 0) {//数据长度为0，不是syn和fin报文
        if (rcv_win == 0) {
            return seg->seq == tcp->recv.nxt;
        } else {
            int v = TCP_SEQ_LE(tcp->recv.nxt, seg->seq) && TCP_SEQ_LT(seg->seq, tcp->recv.nxt + rcv_win);
            return v;
        }
    } else {
        if (rcv_win == 0) {
            return 0; 
        } else {
            int v = TCP_SEQ_LE(tcp->recv.nxt, seg->seq) && TCP_SEQ_LT(seg->seq, tcp->recv.nxt + rcv_win);
            uint32_t last = seg->seq + seg->seq_len -1;
            v |= TCP_SEQ_LE(tcp->recv.nxt, last) && TCP_SEQ_LT(last, tcp->recv.nxt + rcv_win);
            return v;
        }
    }
}
net_err_t tcp_in (pktbuf_t *buf, ipaddr_t *src, ipaddr_t *dest) {
    //根据不同状态做不同处理，函数调用表
    static const tcp_proc_t tcp_state_proc[] = {
        [TCP_STATE_CLOSED] = tcp_closed_in,
        [TCP_STATE_SYN_SENT] = tcp_syn_sent_in,
        [TCP_STATE_ESTABLISHED] = tcp_established_in,
        [TCP_STATE_FIN_WAIT_1] = tcp_fin_wait_1_in,
        [TCP_STATE_FIN_WAIT_2] = tcp_fin_wait_2_in,
        [TCP_STATE_CLOSING] = tcp_closing_in,
        [TCP_STATE_TIME_WAIT] = tcp_time_wait_in,
        [TCP_STATE_CLOSE_WAIT] = tcp_close_wait_in,
        [TCP_STATE_LAST_ACK] = tcp_last_ack_in,      
        [TCP_STATE_LISTEN] = tcp_listen_in,
        [TCP_STATE_SYN_RECVD] = tcp_syn_recvd_in,
    };
    
    //必须设置包头连续性，忘了。。。而且不能获取整个头部的大小，要从包头的shdr中获取，该字段可能不在第一个数据块中
    if (pktbuf_set_cont(buf, sizeof(tcp_hdr_t)) < 0) {
        dbg_error(DBG_TCP, "set pkt cont err");
        return -1;
    }
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
    tcp_display_pkt("tcp in", tcp_hdr, buf);
    tcp_seg_t seg;
    tcp_set_init(&seg, buf, dest, src);

    //查找合适的tcp控制块处理
    tcp_t *tcp = tcp_find(dest, tcp_hdr->dport, src, tcp_hdr->sport);
    if (!tcp) {
        dbg_error(DBG_TCP, "no tcp found");
        //没有找到合适的tcp处理则发送reset分节
        tcp_send_reset(&seg);
        tcp_show_list();
        return NET_ERR_NONE;
    }
    //重启保活机制
    if (tcp->flags.keep_enable) {
        tcp_keepalive_restart(tcp);
    }
    

    net_err_t err = pktbuf_seek(buf, tcp_hdr_size(tcp_hdr));
    if (err < 0) {
        dbg_error(DBG_TCP, "pktbuf seek err");
        return NET_ERR_SIZE;
    }

    if((tcp->state != TCP_STATE_CLOSED) && (TCP_STATE_SYN_SENT  != tcp->state) && (tcp->state != TCP_STATE_LISTEN))
    {
        if (!tcp_seq_acceptable(tcp, &seg)) {
            dbg_info(DBG_TCP, "seq err");
            goto free;
        }
    }
    //查找函数调用表
    tcp_state_proc[tcp->state](tcp, &seg);

    tcp_show_info("after tcp in", tcp);
    // tcp_show_list();
free:
    pktbuf_free(buf);

    return NET_ERR_OK;


}
static int copy_data_to_recvbuf (tcp_t *tcp, tcp_seg_t *seg) {
    int doffset = seg->seq - tcp->recv.nxt;
    if (seg->data_len && (doffset == 0)) {
        return tcp_buf_write_recv(&tcp->recv.buf, doffset, seg->buf, seg->data_len);
    }
    return 0;
}

net_err_t tcp_data_in (tcp_t *tcp, tcp_seg_t *seg) {
    
    int size = copy_data_to_recvbuf(tcp, seg);
    if (size < 0) {
        dbg_error(DBG_TCP, "copy data to recvbuf err");
        return NET_ERR_NONE;
    }
    int wakeup = 0;

    if (size) {
        tcp->recv.nxt += size;
        wakeup++;
    }
    tcp_hdr_t *hdr = seg->hdr;
    if (hdr->f_fin && (tcp->recv.nxt == seg->seq)) { 
        tcp->flags.fin_in = 1;  //只有下一个要接受的字节为fin报文才置位
        tcp->recv.nxt++;
        wakeup++;
    }


    if (wakeup) {
        //if (hdr->f_fin) {//不能直接通知应用结束， 数据可能没有接收完整，比如之前的数据丢了就接受到fin，还需要等待对方重传
        if (tcp->flags.fin_in) {
            //对端关闭，全部唤醒，不支持半关闭
            sock_wakeup(&tcp->base, SOCK_WAIT_ALL, NET_ERR_CLOSE);
        } else {
            //有数据到达，唤醒读进程
            sock_wakeup(&tcp->base, SOCK_WAIT_READ, NET_ERR_OK);
        }
        tcp_send_ack(tcp, seg);
    }

    return NET_ERR_OK;
}