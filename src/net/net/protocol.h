
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define NET_PORT_EMPTY 0
typedef enum _protocol_t {
    NET_PROTOCOL_ARP = 0x0806,     // ARP协议
    NET_PROTOCOL_IPv4 = 0x0800,      // IP协议
    NET_PROTOCOL_ICMPv4 = 0x1,
    NET_PROTOCOL_UDP = 0x11,
    NET_PROTOCOL_TCP = 0x06,

}protocol_t;

#endif 