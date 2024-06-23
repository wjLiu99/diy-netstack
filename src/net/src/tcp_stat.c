#include "tcp_stat.h"
#include "tcp_out.h"

const char * tcp_state_name (tcp_state_t state) {
    static const char * state_name[] = {
        [TCP_STATE_FREE] = "FREE",
        [TCP_STATE_CLOSED] = "CLOSED",
        [TCP_STATE_LISTEN] = "LISTEN",
        [TCP_STATE_SYN_SENT] = "SYN_SENT",
        [TCP_STATE_SYN_RECVD] = "SYN_RCVD",
        [TCP_STATE_ESTABLISHED] = "ESTABLISHED",
        [TCP_STATE_FIN_WAIT_1] = "FIN_WAIT_1",
        [TCP_STATE_FIN_WAIT_2] = "FIN_WAIT_2",
        [TCP_STATE_CLOSING] = "CLOSING",
        [TCP_STATE_TIME_WAIT] = "TIME_WAIT",
        [TCP_STATE_CLOSE_WAIT] = "CLOSE_WAIT",
        [TCP_STATE_LAST_ACK] = "LAST_ACK",

        [TCP_STATE_MAX] = "UNKNOWN",
    };

    if (state >= TCP_STATE_MAX) {
        state = TCP_STATE_MAX;
    }
    return state_name[state];
}

void tcp_set_state (tcp_t * tcp, tcp_state_t state) {
    tcp->state = state;

    tcp_show_info("tcp set state", tcp);
}


net_err_t tcp_closed_in(tcp_t *tcp, tcp_seg_t *seg) {
    return NET_ERR_OK;
}
net_err_t tcp_syn_sent_in(tcp_t *tcp, tcp_seg_t *seg) {
    tcp_hdr_t *hdr = seg->hdr;
    //检查数据包是否在初始序号之后和待发送序号之前
    if ((hdr->ack - tcp->send.iss <= 0) || (hdr->ack - tcp->send.nxt > 0)) {
        dbg_warning(DBG_TCP, "%s: ack err", tcp_state_name(tcp->state));
        //发送reset分节
        return tcp_send_reset(seg);
    }

    //检查收到是否是reset报文
    if (hdr->f_rst) {
        if (!hdr->f_ack) //是否是对前面发出报文的确认，不是直接退出
        {
            return NET_ERR_OK;
        }
        //通知上层收到reset报文
        return tcp_abort(tcp, NET_ERR_RESET);

    }
    //对端也要建立连接,设置接收队列参数
    if (hdr->f_syn) {
        tcp->recv.iss = hdr->seq;
        tcp->recv.nxt = hdr->seq + 1;
        tcp->flags.irs_valid = 1;
        if (hdr->ack) { //如果ack为0是同时打开连接的情况
            tcp_ack_process(tcp, seg);
        }

        if (hdr->f_ack) {
            tcp_send_ack(tcp, seg);
            tcp_set_state(tcp, TCP_STATE_ESTABLISHED);
            sock_wakeup(&tcp->base.conn_wait, SOCK_WAIT_CONN, NET_ERR_OK);
        } else {
            //同时打开，四次握手,发送时ack位会为1，因为已经收到了对方发来的报文，irs_vaild位为1
            tcp_set_state(tcp, TCP_STATE_SYN_RECVD);
            tcp_send_syn(tcp);
        }
       

    }
    return NET_ERR_OK;
}
net_err_t tcp_established_in(tcp_t *tcp, tcp_seg_t *seg) {
    return NET_ERR_OK;
}
net_err_t tcp_close_wait_in (tcp_t * tcp, tcp_seg_t * seg){
    return NET_ERR_OK;
}
net_err_t tcp_last_ack_in (tcp_t * tcp, tcp_seg_t * seg){
    return NET_ERR_OK;
}
net_err_t tcp_fin_wait_1_in(tcp_t * tcp, tcp_seg_t * seg){
    return NET_ERR_OK;
}
net_err_t tcp_fin_wait_2_in(tcp_t * tcp, tcp_seg_t * seg){
    return NET_ERR_OK;
}
net_err_t tcp_closing_in (tcp_t * tcp, tcp_seg_t * seg){
    return NET_ERR_OK;
}
net_err_t tcp_time_wait_in (tcp_t * tcp, tcp_seg_t * seg){
    return NET_ERR_OK;
}
net_err_t tcp_listen_in(tcp_t *tcp, tcp_seg_t *seg){
    return NET_ERR_OK;
}
net_err_t tcp_syn_recvd_in(tcp_t *tcp, tcp_seg_t *seg){
    return NET_ERR_OK;
}