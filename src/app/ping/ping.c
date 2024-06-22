#include "ping.h"
#include "arpa/inet.h"
#include "sys_plat.h"
#include "socket.h"
#include "net_api.h"

static uint16_t checksum(void* buf, uint16_t len) {
    uint16_t* curr_buf = (uint16_t*)buf;
    uint32_t checksum = 0;

    while (len > 1) {
        checksum += *curr_buf++;
        len -= 2;
    }

    if (len > 0) {
        checksum += *(uint8_t*)curr_buf;
    }

    // 注意，这里要不断累加。不然结果在某些情况下计算不正确
    uint16_t high;
    while ((high = checksum >> 16) != 0) {
        checksum = high + (checksum & 0xffff);
    }

    return (uint16_t)~checksum;
}


void ping_run (ping_t *ping, const char *dest, int count, int size, int interval) {
    static int start_id = 0;
    plat_printf("ping, ip: %s, count: %d\n",dest, count);

    int s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if(s < 0){
        plat_printf("ping: open socket error\n");
        return;
    }
    //设置超时
    // struct timeval tmo;
    // tmo.tv_sec = 0;
    // tmo.tv_usec = 0;
    // setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tmo, sizeof(tmo));

    struct sockaddr_in addr;
    plat_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(dest);
    addr.sin_port = 0;
    connect(s, (const struct sockaddr *)&addr, sizeof(addr));
    size = size > PING_BUF_SIZE ? PING_BUF_SIZE : size;

    //填充数据包
    for (int i = 0; i < size; i++) {
        ping->req.buf[i] = i;
    }
    int total_size = sizeof(icmp_hdr_t) + size;

    //循环发送
    for (int i = 0, seq = 0; i < count; i++, seq++) {
        ping->req.icmp_hdr.type = 8;
        ping->req.icmp_hdr.checksum = 0;
        ping->req.icmp_hdr.code = 0;
        ping->req.icmp_hdr.id = start_id;
        ping->req.icmp_hdr.seq = seq;
        ping->req.icmp_hdr.checksum = checksum(&ping->req, total_size);


        int len = send(s, (const char *)&ping->req, total_size, 0);
        if (len < 0) {
            plat_printf("seng ping req failed\n");
            break;
        }
        clock_t time = clock();

        plat_memset(&ping->reply, 0, sizeof(ping->reply));
        do {
            struct sockaddr_in rv_addr;
            socklen_t rv_len = sizeof(rv_addr);

            //会接收到别的数据包
            len = recv(s, (char *)&ping->reply, sizeof(ping->reply), 0);
            if (len < 0) {
                plat_printf("ping timeout\n");
                break;
            }
            if ((ping->reply.icmp_hdr.id == ping->req.icmp_hdr.id) && (ping->reply.icmp_hdr.seq == ping->req.icmp_hdr.seq)) {
                break;
            }

        }while (1);

        if (len > 0) {
            int recv_size = len - sizeof(ip_hdr_t) - sizeof(icmp_hdr_t);
            if (plat_memcmp(ping->req.buf, ping->reply.buf, recv_size)) {
                plat_printf("recv data err\n");
                continue;
            }

            ip_hdr_t *iphdr = &ping->reply.ip_hdr;
            int send_size = size;
            if (send_size == recv_size) {
                plat_printf("reply from %s: byte = %d, ", inet_ntoa(addr.sin_addr), send_size);
            } else {
                plat_printf("reply from %s: byte = %d(send = %d), ", inet_ntoa(addr.sin_addr), recv_size, send_size);
            }

            int diff_ms = (clock() - time) / (CLOCKS_PER_SEC / 1000);
            if (diff_ms < 1) {
                plat_printf("  time < 1ms, TTL = %d\n", iphdr->ttl);
            } else {
                plat_printf("  time = %dms, TTL = %d\n", diff_ms, iphdr->ttl);
            }
        }
        
    }
    close(s);

}