#include "tcp_stat.h"
#include "tcp_out.h"
#include "tcp_in.h"

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
    //关闭状态，直接回复reset
    if (!seg->hdr->f_rst) {
         tcp_send_reset(tcp);
    }
   
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
            sock_wakeup(&tcp->base, SOCK_WAIT_CONN, NET_ERR_OK);
        } else {
            //同时打开，四次握手,发送时ack位会为1，因为已经收到了对方发来的报文，irs_vaild位为1
            tcp_set_state(tcp, TCP_STATE_SYN_RECVD);
            tcp_send_syn(tcp);
        }
       

    }
    return NET_ERR_OK;
}
net_err_t tcp_established_in(tcp_t *tcp, tcp_seg_t *seg) {
    tcp_hdr_t *hdr = seg->hdr;

    //是否收到reset报文
    if (hdr->f_rst) {
        dbg_warning(DBG_TCP, "recv a reset");
        return tcp_abort(tcp, NET_ERR_RESET);

    }

    //是否是syn报文
    if (hdr->f_syn) {
        dbg_warning(DBG_TCP, "recv a syn");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    //处理ack
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "ack process err");
        return NET_ERR_UNREACH;
    }
    //提取数据
    tcp_data_in(tcp, seg);

    if (hdr->f_fin) {
        tcp_set_state(tcp, TCP_STATE_CLOSE_WAIT);
    }

    return NET_ERR_OK;
}
net_err_t tcp_close_wait_in (tcp_t * tcp, tcp_seg_t * seg){
         tcp_hdr_t *hdr = seg->hdr;
      //是否收到reset报文
    if (hdr->f_rst) {
        dbg_warning(DBG_TCP, "recv a reset");
        return tcp_abort(tcp, NET_ERR_RESET);

    }

    //是否是syn报文
    if (hdr->f_syn) {
        dbg_warning(DBG_TCP, "recv a syn");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    //处理ack
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "ack process err");
        return NET_ERR_UNREACH;
        
    }
    return NET_ERR_OK;
}



net_err_t tcp_last_ack_in (tcp_t * tcp, tcp_seg_t * seg){
    tcp_hdr_t *hdr = seg->hdr;
      //是否收到reset报文
    if (hdr->f_rst) {
        dbg_warning(DBG_TCP, "recv a reset");
        return tcp_abort(tcp, NET_ERR_RESET);

    }

    //是否是syn报文
    if (hdr->f_syn) {
        dbg_warning(DBG_TCP, "recv a syn");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    //处理ack
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "ack process err");
        return NET_ERR_UNREACH;
    }

    return tcp_abort(tcp, NET_ERR_CLOSE);
}


void tcp_time_wait (tcp_t *tcp) {
    tcp_set_state(tcp, TCP_STATE_TIME_WAIT);
}

net_err_t tcp_fin_wait_1_in(tcp_t * tcp, tcp_seg_t * seg){
    tcp_hdr_t *hdr = seg->hdr;
      //是否收到reset报文
    if (hdr->f_rst) {
        dbg_warning(DBG_TCP, "recv a reset");
        return tcp_abort(tcp, NET_ERR_RESET);

    }

    //是否是syn报文
    if (hdr->f_syn) {
        dbg_warning(DBG_TCP, "recv a syn");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    //处理ack
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "ack process err");
        return NET_ERR_UNREACH;
        
    }
    //还函数内会发送ack
    tcp_data_in(tcp, seg);
    if(tcp->flags.fin_out == 0) { //发送的fin报文对方已经确认收到，走正常的四次挥手
        
        if (hdr->f_fin) {
        //如果收到fin报文切换到timewait
        tcp_time_wait(tcp);
        } else {
            tcp_set_state(tcp, TCP_STATE_FIN_WAIT_2);
        }
    } else { //fin报文对方没收到，同时关闭
        tcp_set_state(tcp, TCP_STATE_CLOSING);
    }
    

    return NET_ERR_OK;
}



net_err_t tcp_fin_wait_2_in(tcp_t * tcp, tcp_seg_t * seg){
    tcp_hdr_t *hdr = seg->hdr;
      //是否收到reset报文
    if (hdr->f_rst) {
        dbg_warning(DBG_TCP, "recv a reset");
        return tcp_abort(tcp, NET_ERR_RESET);

    }

    //是否是syn报文
    if (hdr->f_syn) {
        dbg_warning(DBG_TCP, "recv a syn");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    //处理ack
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "ack process err");
        return NET_ERR_UNREACH;
        
    }
    //函数内会发送ack,如果有数据对数据进行处理
    tcp_data_in(tcp, seg);

    if (hdr->f_fin) {
        //如果收到fin报文切换到timewait
        tcp_time_wait(tcp);
    }
    return NET_ERR_OK;
}
net_err_t tcp_closing_in (tcp_t * tcp, tcp_seg_t * seg){
    tcp_hdr_t *hdr = seg->hdr;
      //是否收到reset报文
    if (hdr->f_rst) {
        dbg_warning(DBG_TCP, "recv a reset");
        return tcp_abort(tcp, NET_ERR_RESET);

    }

    //是否是syn报文
    if (hdr->f_syn) {
        dbg_warning(DBG_TCP, "recv a syn");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    //处理ack
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "ack process err");
        return NET_ERR_UNREACH;
        
    }

    //如果fin报文已经被对方接收，进入timewait状态
    if (tcp->flags.fin_out == 0){
        tcp_time_wait(tcp);
    }
    return NET_ERR_OK;
}
net_err_t tcp_time_wait_in (tcp_t * tcp, tcp_seg_t * seg){
     tcp_hdr_t *hdr = seg->hdr;
      //是否收到reset报文
    if (hdr->f_rst) {
        dbg_warning(DBG_TCP, "recv a reset");
        return tcp_abort(tcp, NET_ERR_RESET);

    }

    //是否是syn报文
    if (hdr->f_syn) {
        dbg_warning(DBG_TCP, "recv a syn");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    //处理ack
    if (tcp_ack_process(tcp, seg) < 0) {
        dbg_warning(DBG_TCP, "ack process err");
        return NET_ERR_UNREACH;
        
    }
    //该状态不能接收任何数据
    // tcp_data_in(tcp, seg);

    if (hdr->f_fin) {
        tcp_send_ack(tcp, seg);
        //再次收到fin报文说明上次发送的ack丢失了，需要重新加入timewait状态，重新计时2msl
        tcp_time_wait(tcp);
    }

    return NET_ERR_OK;
    return NET_ERR_OK;
}



net_err_t tcp_listen_in(tcp_t *tcp, tcp_seg_t *seg){
    return NET_ERR_OK;
}
net_err_t tcp_syn_recvd_in(tcp_t *tcp, tcp_seg_t *seg){
    return NET_ERR_OK;
}