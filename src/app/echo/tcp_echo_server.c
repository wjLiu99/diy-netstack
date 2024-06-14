#include "tcp_echo_server.h"
#include <arpa/inet.h>
#include "sys_plat.h"

void tcp_echo_server_start(int port){
    plat_printf("tcp echo server port: %d\n", port);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0){
        plat_printf("tcp echo client: open socket error");
        goto end;
    }

    struct sockaddr_in ser_addr, cli_addr;
    socklen_t len = sizeof(cli_addr);
    plat_memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = inet_addr("192.168.133.102");
    ser_addr.sin_port = htons(port);

    if(bind(s, (const struct sockaddr *)&ser_addr, sizeof(ser_addr)) < 0){
        plat_printf("bind error");
        goto end;
    }

    listen(s, 5);

    while(1){
        int cfd = accept(s, (struct sockaddr *)&cli_addr, &len);

        if(cfd < 0){
            plat_printf("accept error");
            break;
        }

        plat_printf("tcp echo server: connect ip: %s, port: %d\n",
            inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port)
        );

        char buf[125];
        ssize_t size;
        while((size = recv(cfd, buf, sizeof(buf), 0)) > 0 ){
            plat_printf("recv size: %d\n",(int)size);
            send(cfd, buf, size, 0);
        }
        close(cfd);

    }

 
end:
    if(s >= 0){
        close(s);
    }

}

