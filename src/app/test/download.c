#include <stdio.h>
#include <string.h>
#include "net_plat.h"
// #include <arpa/inet.h>

#include "net_err.h"
#include "net_api.h"





//简单的从对方客户机进行下载，以便测试TCP接收

void download_test (const char * filename, int port) {
    printf("try to download %s from %s: %d\n", filename, friend0_ip, port);

    // 创建服务器套接字，使用IPv4，数据流传输
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("create socket error\n");
		return;
	}
    int keepalive = 1;
    int keepidle = 3;           // 空间一段时间后
    int keepinterval = 1;       // 时间多长时间
    int keepcount = 10;         // 重发多少次keepalive包
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive ));
    setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (void*)&keepidle , sizeof(keepidle ));
    setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepinterval , sizeof(keepinterval ));
    setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void *)&keepcount , sizeof(keepcount ));

    // 绑定本地地址
    struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(friend0_ip);
    if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
		printf("connect error\n");
        close(sockfd);
        return;
    }

    FILE * file = fopen(filename, "wb");
    if (file == (FILE *)0) {
        printf("open file failed.\n");
        goto failed;
    }

    char buf[8192];
    ssize_t total_size = 0;
    int rcv_size;
    while ((rcv_size = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, rcv_size, file);
        fflush(file);
        printf(".");
        total_size += rcv_size;
    }
    if ((rcv_size == 0) || (rcv_size) == NET_ERR_EOF) {
        // 接收完毕
        fclose(file);
        close(sockfd);
        printf("rcv file size: %d\n", (int)total_size);
        return;
    }
failed:
    printf("rcv file error\n");
    close(sockfd);
    fclose(file);
    return;
}