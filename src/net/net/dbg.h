#ifndef DBG_H
#define DBG_H

#define DBG_STYLE_RESET       "\033[0m"       // 复位显示
#define DBG_STYLE_ERROR       "\033[31m"      // 红色显示
#define DBG_STYLE_WARNING     "\033[33m"      // 黄色显示

#define DBG_LEVEL_NONE 0
#define DBG_LEVEL_ERROR 1
#define DBG_LEVEL_WARNING 2
#define DBG_LEVEL_INFO 3

void dbg_print(int m_level, int s_level, const char *file, const char *func, int line, const char *fmt, ...);
#define dbg_info(dbg_level, fmt, ...) dbg_print(dbg_level, DBG_LEVEL_INFO, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define dbg_warning(dbg_level, fmt, ...) dbg_print(dbg_level, DBG_LEVEL_WARNING, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define dbg_error(dbg_level, fmt, ...) dbg_print(dbg_level, DBG_LEVEL_ERROR, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)


#define dbg_assert(expr, msg){\
    if(!(expr)){\
        dbg_print(DBG_LEVEL_ERROR, DBG_LEVEL_ERROR, __FILE__,_ _FUNCTION__, __LINE__, "assert_failed:"#expr","msg);\
        while(1);\
    }\
}
#endif