/*
	GIT: https://github.com/mkuch95
*/


#pragma once
#include <osapi.h>

//#define LOG_TRACE
#define LOG_INFO
#define LOG_WARN
#define LOG_ERROR


#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)



#ifdef LOG_TRACE
#define log_trace(format, ...) os_printf("%s:%s Trace: "format"\n", __FILENAME__, __FUNCTION__, ##__VA_ARGS__)
#else
#define log_trace(format, ...)
#endif // LOG_TRACE

#ifdef LOG_INFO
#define log_info(format, ...) os_printf("%s:%s Info: "format"\n", __FILENAME__, __FUNCTION__, ##__VA_ARGS__)
#else
#define log_info(format, ...)
#endif // LOG_INFO

#ifdef LOG_WARN
#define log_warn(format, ...) os_printf("%s:%s Warning: "format"\n", __FILENAME__, __FUNCTION__, ##__VA_ARGS__)
#else
#define log_warn(format, ...)
#endif // LOG_WARN

#ifdef LOG_ERROR
#define log_error(format, ...) os_printf("%s:%s Error: "format"\n", __FILENAME__, __FUNCTION__, ##__VA_ARGS__)
#else
#define log_error(format, ...)
#endif // LOG_ERROR
