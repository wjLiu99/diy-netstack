﻿#include <stdio.h>
#include"sys_plat.h"
#include "net.h"
#include "netif_pcap.h"
#include "dbg.h"
#include "nlist.h"
#include "mblock.h"
#include "nlocker.h"
#include "pktbuf.h"
#include "netif.h"
#include "ntools.h"

pcap_data_t netdev0_data = {.ip = netdev0_phy_ip, .hwaddr = netdev0_hwaddr};

net_err_t netdev_init(void){
	dbg_info(DBG_NETIF, "netdev init ...");
	

    netif_t *netif = netif_open("netif0", &netdev_ops, &netdev0_data);
    if (!netif) {
        dbg_error(DBG_NETIF, "netif0 open err");
        return NET_ERR_NONE;
    }

    ipaddr_t ip, netmask, gateway;
    ipaddr_from_str(&ip, netdev0_ip);
    ipaddr_from_str(&netmask, netdev0_mask);
	ipaddr_from_str(&gateway, netdev0_gw);

    netif_set_addr(netif, &ip, &netmask, &gateway);

    netif_set_active(netif);

    pktbuf_t *buf = pktbuf_alloc(32);
	pktbuf_fill(buf, 0x53, 32);
    netif_out(netif, (ipaddr_t *)0, buf);
    dbg_info(DBG_NETIF, "netif0 init done");
    return NET_ERR_OK;

}
#define DBG_TEST DBG_LEVEL_INFO

typedef struct _tnode_t{
	int id;
	nlist_node_t node;
}tnode_t;
void list_test(){
	tnode_t node[4];
	nlist_t list;
	nlist_init(&list);
	for(int i = 0; i < 4; i++){
		node[i].id = i;
		nlist_insert_after(&list, nlist_first(&list), &node[i].node);
	}
	nlist_node_t *p;
	nlist_for_each(p, &list){
		tnode_t *tnode = nlist_entry(p, tnode_t, node);
		plat_printf("id = %d\n", tnode->id);
	}

	for (int i = 0; i < 4; i++){
		p = nlist_remove_last(&list);
		tnode_t *tnode = nlist_entry(p, tnode_t, node);
		plat_printf("remove id = %d\n", tnode->id);
	}

}


void mblock_test(){
	mblock_t blist;
	static uint8_t buf[100][10];
	mblock_init(&blist, buf, 100, 10, NLOCKER_THREAD);
	void *temp[10];
	for(int i = 0;i < 10; i++){
		temp[i] = mblock_alloc(&blist, 0);
		plat_printf("block: %p, free count: %d\n", temp[i], mblock_free_cnt(&blist));
	}

	for (int i = 0; i < 10; i++){
		mblock_free(&blist, temp[i]);
		plat_printf("free count: %d\n",mblock_free_cnt(&blist));
	}
	mblock_destory(&blist);
}

void pktbuf_test(){
	pktbuf_t * buf = pktbuf_alloc(2000);
	for (int i = 0; i < 20; i++){
		pktbuf_add_header(buf, 33, 0);
	}
		for (int i = 0; i < 20; i++){
		pktbuf_remove_header(buf, 33);
	}

	buf = pktbuf_alloc(8);

	pktbuf_resize(buf, 32);

	pktbuf_resize(buf, 200);
	pktbuf_resize(buf, 400);
	pktbuf_resize(buf, 200);
	pktbuf_resize(buf, 20);
	pktbuf_resize(buf, 14);


	pktbuf_join(buf, pktbuf_alloc(14));
	pktbuf_join(buf, pktbuf_alloc(14));
	pktbuf_join(buf, pktbuf_alloc(14));
	pktbuf_join(buf, pktbuf_alloc(14));

	pktbuf_set_cont(buf, 20);
	pktbuf_set_cont(buf, 28);
	pktbuf_set_cont(buf, 42);
	pktbuf_set_cont(buf, 56);
	pktbuf_set_cont(buf, 66);
	pktbuf_join(buf, pktbuf_alloc(1000));
	pktbuf_reset_acc(buf);
	static uint16_t temp[1000];
	static uint16_t read_temp[1000];

	for (int i = 0; i < 1024; i++) {
		temp[i] = i;
	}
	pktbuf_reset_acc(buf);
	pktbuf_write(buf, (uint8_t *)temp, pktbuf_total(buf));      // 16位的读写
	plat_memset(read_temp, 0, sizeof(read_temp));
	pktbuf_reset_acc(buf);
	pktbuf_read(buf, (uint8_t*)read_temp, pktbuf_total(buf));
	if (plat_memcmp(temp, read_temp, pktbuf_total(buf)) != 0) {
		printf("not equal.");
		exit(-1);
	}
	plat_memset(read_temp, 0, sizeof(read_temp));
	pktbuf_seek(buf, 18 * 2);
	pktbuf_read(buf, (uint8_t*)read_temp, 56);
	if (plat_memcmp(temp + 18, read_temp, 56) != 0) {
		printf("not equal.");
		exit(-1);
	}

    // 定位跨一个块的读写测试, 从170开始读，读56
	plat_memset(read_temp, 0, sizeof(read_temp));
	pktbuf_seek(buf, 85 * 2);
	pktbuf_read(buf, (uint8_t*)read_temp, 256);
	if (plat_memcmp(temp + 85, read_temp, 256) != 0) {
		printf("not equal.");
		exit(-1);
	}


	pktbuf_t* dest = pktbuf_alloc(1024);
	pktbuf_seek(buf, 200);      // 从200处开始读
	pktbuf_seek(dest, 600);     // 从600处开始写
	pktbuf_copy(dest, buf, 122);    // 复制122个字节

    // 重新定位到600处开始读
	plat_memset(read_temp, 0, sizeof(read_temp));
	pktbuf_seek(dest, 600);
	pktbuf_read(dest, (uint8_t*)read_temp, 122);    // 读122个字节
	if (plat_memcmp(temp + 100, read_temp, 122) != 0) { // temp+100，实际定位到200字节偏移处
		printf("not equal.");
		exit(-1);
	}

	pktbuf_seek(dest, 0);
	pktbuf_fill(dest, 53, pktbuf_total(dest));

	plat_memset(read_temp, 0, sizeof(read_temp));
	pktbuf_seek(dest, 0);
	pktbuf_read(dest, (uint8_t*)read_temp, pktbuf_total(dest));
	for (int i = 0; i < pktbuf_total(dest); i++) {
		if (((uint8_t *)read_temp)[i] != 53) {
			printf("not equal.");
			exit(-1);
		}
	}

	pktbuf_free(dest);
	pktbuf_free(buf);       // 可以进去调试，在退出函数前看下所有块是否全部释放完毕

	
}
void netif_test(){
	
}
void base_test(){

}
int main (void) {
	
	net_init();
	base_test();
	netdev_init();
	net_start();
	
	

	dbg_info(DBG_TEST, "info");
	dbg_warning(DBG_TEST, "warning");
	dbg_error(DBG_TEST, "error");

	
	while(1){
		sys_sleep(10);
	}
	return 0;
}