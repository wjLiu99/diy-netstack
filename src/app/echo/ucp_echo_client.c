
#include <string.h>
#include <stdio.h>
// #include "net_api.h"
#include"sys_plat.h"
#include <arpa/inet.h>


int udp_echo_client_start(const char* ip, int port) {
    printf("udp echo client, ip: %s, port: %d\n", ip, port);
    printf("Enter quit to exit\n");

    // 创建套接字，使用流式传输，即tcp
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        printf("open socket error");
        goto end;
    }

    // 连接的服务地址和端口
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);   // ip地址，注意大小端转换
    server_addr.sin_port = htons(port);             // 取端口号，注意大小端转换



    // 循环，读取一行后发出去
    printf(">>");
    char buf[128];
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        // 如果接收到quit，则退出断开这个过程
        if (strncmp(buf, "quit", 4) == 0) {
            break;
        }
 
        // 将数据写到服务器中，不含结束符
        // 在第一次发送前会自动绑定到本地的某个端口和地址中
        size_t total_len = strlen(buf);

        ssize_t size = sendto(s, buf, total_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

        if (size < 0) {
            printf("send error");
            goto end;
        }

        // 读取回显结果并显示到屏幕上，不含结束符
        memset(buf, 0, sizeof(buf));

        struct sockaddr_in remote_addr;
        socklen_t addr_len;
        size = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&remote_addr, &addr_len);

        if (size < 0) {
            printf("send error");
            goto end;
        }
        buf[sizeof(buf) - 1] = '\0';        // 谨慎起见，写结束符

        // 在屏幕上显示出来
        printf("%s", buf);
        printf(">>");
    }

    // 关闭连接
end:
    if (s >= 0) {
        close(s);
    }
    return -1;
}

