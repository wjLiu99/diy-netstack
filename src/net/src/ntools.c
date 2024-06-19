#include "ntools.h"
#include "dbg.h"
#include "pktbuf.h"

static int is_little_endian(void) {
    // 存储字节顺序，从低地址->高地址
    // 大端：0x12, 0x34;小端：0x34, 0x12
    uint16_t v = 0x1234;
    uint8_t* b = (uint8_t*)&v;

    // 取最开始第1个字节，为0x34,即为小端，否则为大端
    return b[0] == 0x34;
}

net_err_t tools_init(void) {
    dbg_info(DBG_TOOLS, "init tools.");

    // 实际是小端，但配置项非小端，检查报错
    if (is_little_endian()  != NET_ENDIAN_LITTLE) {
        dbg_error(DBG_TOOLS, "check endian faild.");
        return NET_ERR_SYS;
    }
    
    dbg_info(DBG_TOOLS, "done.");
    return NET_ERR_OK;
}

uint16_t checksum16 (void *buf, uint16_t len, uint32_t pre_sum, int complement) {
    uint16_t *cur_buf = (uint16_t *)buf;
    uint32_t checksum = pre_sum;
    while (len > 1) {
        checksum += *cur_buf++;
        len -= 2;
    }

    if (len > 0) {
        checksum += *(uint8_t *)cur_buf;
    }
    //把高16位加到低16位
    uint16_t high;
    while ((high = checksum >> 16) != 0) {
        checksum = high + (checksum & 0xffff);

    }

    return complement ? (uint16_t)~checksum : (uint16_t)checksum;

}

