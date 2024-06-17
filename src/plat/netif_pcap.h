//网络接口层

#ifndef NETIF_PCAP_H
#define NETIF_PACP_H

#include "net_err.h"
#include "netif.h"

extern const netif_ops_t netdev_ops;

typedef struct _pcap_data_t {
    const char *ip;
    const uint8_t *hwaddr;
} pcap_data_t;

net_err_t netif_pcap_open (struct _netif_t *netif, void *data);
#endif