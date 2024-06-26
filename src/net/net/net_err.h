#ifndef NET_ERR_H
#define NET_ERR_H

typedef enum _net_err_t{
    NET_ERR_WAIT = 1,
    NET_ERR_OK = 0,
    NET_ERR_SYS = -1,
    NET_ERR_MEM = -2,
    NET_ERR_FULL = -3,
    NET_ERR_TMO = -4,
    NET_ERR_SIZE = -5,
    NET_ERR_PARAM = -6,
    NET_ERR_NONE = -7,
    NET_ERR_STATE = -8,
    NET_ERR_IO = -9,
    NET_ERR_EXIST = -10,
    NET_ERR_UNSUPPORT = -11,
    NET_ERR_UNREACH = -12,
    NET_ERR_CHECKSUM = -13,
    NET_ERR_BUF = -14,
    NET_ERR_RESET = -15,
    NET_ERR_BROKEN = -16,
    NET_ERR_CLOSE = -17,
    NET_ERR_EOF = -18,
    NET_ERR_UNKNOWN = -19,
    NET_ERR_ADDR = -20,
    
    
}net_err_t;
#endif