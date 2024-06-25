#include "nlist.h"

void nlist_init (nlist_t *list){
    list->first = list->last = (nlist_node_t *)0;
    list->count = 0;

}


void nlist_insert_first (nlist_t *list, nlist_node_t *node){
    node->pre = (nlist_node_t *)0;
    if(!list->first){
        node->next = (nlist_node_t *)0;
        list->first = list->last = node;

    }else{
        node->next = list->first;
        list->first->pre = node;
        list->first = node;

    }
    list->count++;
}

nlist_node_t *nlist_remove (nlist_t *list, nlist_node_t *node){
    //判断节点是否在链表里
    int exist = 0;
    if(list->first == node){
        list->first = node->next;
        exist = 1;
    }

    if(list->last == node){
        list->last = node->pre;
        exist = 1;
    }

    if(node->pre){
        node->pre->next = node->next;
        exist = 1;
    }

    if(node->next){
        node->next->pre = node->pre;
        exist = 1;
    }
    node->pre = node->next = (nlist_node_t *)0;
    if (exist) {
        list->count--;
    }
    
    return node;
}


void nlist_insert_last (nlist_t *list, nlist_node_t *node){
    node->next = (nlist_node_t *)0;
    node->pre = list->last;
    if(nlist_is_empty(list)){
        list->first = list->last = node;

    }else{
        list->last->next = node;
        list->last = node;
    }
    list->count++;
}

void nlist_insert_after (nlist_t *list, nlist_node_t *pre, nlist_node_t *node){
    if(!pre || nlist_is_empty(list)){
        nlist_insert_first(list, node);
        return;
    }

    node->next = pre->next;
    node->pre = pre;
    if(pre->next){
        pre->next->pre = node;
    }
    pre->next = node;

    if(list->last == pre){
        list->last = node;
    }
    list->count++;
}