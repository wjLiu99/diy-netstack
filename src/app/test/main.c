#include <stdio.h>
#include"sys_plat.h"
#include "net.h"
#include "netif_pcap.h"
#include "dbg.h"


void thread1_entry(void *arg){
	while(1){
		plat_printf("this is thread1 :%s\n", (char *)arg);
		sys_sleep(1000);
	}
}


void thread2_entry(void *arg){
	while(1){
		plat_printf("this is thread2 :%s\n", (char *)arg);
		sys_sleep(1000);
	}
}

net_err_t netdev_init(void){
	netif_pcap_open();
	return NET_ERR_OK;
}
int main (void) {
	net_init();
	net_start();
	netdev_init();
	
	while(1){
		sys_sleep(10);
	}
	return 0;
}