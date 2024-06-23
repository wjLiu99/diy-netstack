#include "tcp_in.h"
#include "ntools.h"
#include "protocol.h"
#include "tcp_out.h"
#include "tcp_stat.h"

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
    //查找函数调用表
    tcp_state_proc[tcp->state](tcp, &seg);

    tcp_show_info("after tcp in", tcp);
    // tcp_show_list();

    return NET_ERR_OK;


}

net_err_t tcp_data_in (tcp_t *tcp, tcp_seg_t *seg) {
    int wakeup = 0;
    tcp_hdr_t *hdr = seg->hdr;
    if (hdr->f_fin) {
        tcp->recv.nxt++;
        wakeup++;
    }


    if (wakeup) {
        if (hdr->f_fin) {
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