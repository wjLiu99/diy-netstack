#include"tcp_echo_client.h"
#include<arpa/inet.h>
#include"sys_plat.h"

int tcp_echo_client_start(const char* ip, int port){
    plat_printf("tcp echo client, ip: %s, port: %d\n",ip, port);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0){
        plat_printf("tcp echo client: open socket error");
        goto end;
    }
    struct sockaddr_in ser_addr;
    plat_memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = inet_addr(ip);
    ser_addr.sin_port = htons(port);

    if(connect(s, (const struct sockaddr *)&ser_addr, sizeof(ser_addr)) < 0){
        plat_printf("connect error");
        goto end;
    }

    char buf[128];
    plat_printf(">>");
    while(fgets(buf, sizeof(buf), stdin) != NULL ){
        if(send(s, buf, plat_strlen(buf), 0) <= 0){
            plat_printf("write error");
            goto end;
        }

        plat_memset(buf, 0, sizeof(buf));
        int len = recv(s, buf, sizeof(buf) - 1 , 0);
        if(len <= 0){
            plat_printf(" read error ");
            goto end;
        }


        plat_printf("%s\n",buf);
        plat_printf(">>\n");

       
    }
    close(s);
end:
    if(s >= 0){
        close(s);
    }
    return -1;

}