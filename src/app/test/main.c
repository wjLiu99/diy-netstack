#include <stdio.h>
#include"sys_plat.h"
#include "net.h"
#include "netif_pcap.h"
#include "dbg.h"
#include "nlist.h"
#include "mblock.h"
#include "nlocker.h"

net_err_t netdev_init(void){
	netif_pcap_open();
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

	for( int i = 0; i<4;i++){
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
void base_test(){
	mblock_test();
}
int main (void) {
	base_test();
	net_init();
	net_start();
	netdev_init();
	

	dbg_info(DBG_TEST, "info");
	dbg_warning(DBG_TEST, "warning");
	dbg_error(DBG_TEST, "error");

	
	while(1){
		sys_sleep(10);
	}
	return 0;
}