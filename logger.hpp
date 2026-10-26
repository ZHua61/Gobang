/*实现宏函数，用于日志记录与输出*/

#ifndef __M_LOGGER_H__ /*防止头文件重复包含*/
#define __M_LOGGER_H__

#include <stdio.h>
#include <time.h>
#include <pthread.h>


#define INF 0
#define DBG 1
#define ERROR 2
#define DEFAULT_LOG_LEVEL DBG

#define LOG(level, format, ...) do{ \
    if(level < DEFAULT_LOG_LEVEL) break; \
    time_t t=time(NULL);/*获取当前时间*/ \
    struct tm *ltm=localtime(&t);  /*将时间转换为本地时间*/ \
    char tmp[32]={0};  \
    strftime(tmp, 31, "%H:%M:%S", ltm); /*格式化时间*/ \
    fprintf(stdout,"[%p %s %s %d]" format "\n",(void *)pthread_self(),tmp ,__FILE__, __LINE__, ##__VA_ARGS__); \
}while(0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERROR_LOG(format, ...) LOG(ERROR, format, ##__VA_ARGS__)
#endif
