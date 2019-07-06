#ifndef _BENCHMARK_H_
#define _BENCHMARK_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>

#define MAX_SOCK_NUM        10
#define MODENUM             10
#define FILENAME_LEN        50
#define IP_LEN              16
#define MAX_LINE_LEN        2000
#define DIVIDER             5

typedef enum log_level{
	LOGNON=0,
	LOGFAT=1,
	LOGERR=2,
	LOGWAN=3,
	LOGINF=4,
	LOGADT=5,
	LOGDBG=6
}log_level_t;

char level_print[7][10] = { "LOGNON" , \
                            "LOGFAT" , \
                            "LOGERR" , \
                            "LOGWAN" , \
                            "LOGINF" , \
                            "LOGADT" , \
                            "LOGDBG" };
#endif
