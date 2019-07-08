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

#define	timersub(tvp, uvp, vvp)						        \
	do {								                    \
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				            \
			(vvp)->tv_sec--;				                \
			(vvp)->tv_usec += 1000000;			            \
		}							                        \
	} while (0)

#endif
