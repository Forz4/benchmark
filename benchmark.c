/* benchmark for TCPIP transactions by wanghao*/
#include "benchmark.h"
/* global variables */
FILE    *g_fp;                          /* input file pointer */
char    g_inputFileName[FILENAME_LEN];  /* intpu file name */
char    g_ipAddress[16];                /* remote IP*/
int     g_sock4send[MAX_SOCK_NUM];      /* sockets for sending */
int     g_numOfSend;                    /* number of sending sockets */
int     g_remotePorts[MAX_SOCK_NUM];    /* remote ports */
int     g_sock4recv[MAX_SOCK_NUM];      /* sockets for receiving */
int     g_localPorts[MAX_SOCK_NUM];     /* local ports */
int     g_numOfRecv;                    /* number of receiving sockets */
int     g_tps;                          /* transaction per second */
int     g_timeLast;                     /* total time last */
int     g_childPid;                     /* child pid */
log_level_t     g_logLevel;             /* log level */
float   g_percentage;                   /* varies from machinse */
int     g_hexMode;                      /* hex mode */
/* prototypes */
void print_help();
void parse_ports(char *line , int ports[MAX_SOCK_NUM] , int *num);
int start_listen();
void start_send_proc();
void real_send();
void father_signal_handler(int no);
void cal_time_ns(int tps , struct timespec *ts);
void start_recv_proc();
void clean_send_proc();
void clean_recv_proc();
void logp(log_level_t log_level , char *fmt , ...);
void persist_trac( log_level_t log_level , int flag , int msgLength , char *msg);
/* functions */
void print_help()
{
    logp(LOGNON,"benchmark");
    logp(LOGNON,"HELP:");
    logp(LOGNON,"    [-h]          print help page");
    logp(LOGNON,"    [-f filename] to specify input file name , set to STDIN by default");
    logp(LOGNON,"    [-H]          set input mode to Hex , default option is ascii");
    logp(LOGNON,"    [-i ip]       remote IP address , set to 127.0.0.1 by default");
    logp(LOGNON,"    [-r remote_port1,remote_port2,...]");
    logp(LOGNON,"                  to specify remote port(s) , divided by comma");
    logp(LOGNON,"    [-l local_port1,local_port2,...]");
    logp(LOGNON,"                  to specify local listening port(s) , divided by comma");
    logp(LOGNON,"    [-s tps]      transaction per second , set to 10 by default");
    logp(LOGNON,"    [-t time]     to specify total time last , set to 10 by default , valid range [0,999999] , 0 means infinite time");
    logp(LOGNON,"    [-u level]    to specify log level , valid range [0,6] , set to 4-LOGINF by default");
    logp(LOGNON,"                  0-LOGNON , 1-LOGFAT , 2-LOGERR , 3-LOGWAN , 4-LOGINF , 5-LOGADT , 6-LOGDBG");
    logp(LOGNON,"    [-p percent]  to adjust time interval , valid from [1,100] , set to 100 by default.");
}
void parse_ports(char *line , int ports[MAX_SOCK_NUM] , int *num)
{
    int i = 0;
    char *v = NULL;
    v = strtok(line , ",");
    while ( v != NULL && i < MAX_SOCK_NUM ){
        ports[i++] = atoi(v);
        v = strtok( NULL , ",");
    }
    *num = i;
    return;
}
int start_listen()
{
    int i = 0;
    int server_sockfd;
    struct sockaddr_in server_sockaddr;
    struct sockaddr_in client_addr;
    socklen_t socket_len = sizeof(client_addr);
    for ( i = 0 ; i < g_numOfRecv ; i ++){
        logp( LOGDBG , "child starts to listen on local_port[%d] ...",g_localPorts[i]);
        server_sockfd = socket(AF_INET , SOCK_STREAM , 0);
        /*bind*/
        server_sockaddr.sin_family = AF_INET;
        server_sockaddr.sin_port = htons(g_localPorts[i]);
        server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(server_sockfd , (struct sockaddr *)&server_sockaddr , sizeof(server_sockaddr)) == -1){
            logp( LOGERR, "bind err , port[%d]" , g_localPorts[i]);
            return -1;
        }
        /*listen*/
        if(listen(server_sockfd , 1) == -1){
            logp( LOGERR , "listen err , port[%d]" , g_localPorts[i]);
            return -1;
        }
        /*accept*/
        g_sock4recv[i] = accept(server_sockfd , (struct sockaddr *)&client_addr , &socket_len);
        if (g_sock4recv[i] < 0){
            logp( LOGERR , "accept err , port[%d]" , g_localPorts[i]);
            return -1;
        }
        logp( LOGDBG , "child local_port[%d] accept OK",g_localPorts[i]);
    }
    return 0;
}
void start_send_proc()
{
    int i = 0;
    struct sockaddr_in servaddr;    
    int sock_send = 0; 
    /* handle signals */
    signal( SIGALRM , father_signal_handler);
    signal( SIGCHLD , father_signal_handler);
    signal( SIGUSR1 , father_signal_handler);
    /* input file */
    if ( strlen(g_inputFileName) != 0 ){
        g_fp = NULL;
        g_fp = fopen( g_inputFileName , "rb");
        if ( g_fp == NULL ){
            logp( LOGERR , "fopen err , filename[%s]" , g_inputFileName);
            exit(1);
        }
    } 
    else {
        g_fp = stdin;
    }
    /* create socket */
    for ( i = 0 ; i < g_numOfSend ; i ++) {
        sock_send = socket(AF_INET , SOCK_STREAM , 0);
        memset(&servaddr , 0 , sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(g_remotePorts[i]);
        servaddr.sin_addr.s_addr = inet_addr(g_ipAddress);
        while (1){
            if (connect(sock_send , (struct sockaddr*)&servaddr , sizeof(servaddr)) < 0){
                close(sock_send);
                sock_send = socket(AF_INET , SOCK_STREAM , 0);
                usleep(100000);
                continue;
            }
            g_sock4send[i] = sock_send;
            break;
        }
        logp( LOGDBG , "father connect to remote port[%d] OK" ,  g_remotePorts[i]);
    }
    logp( LOGDBG , "father waits for child to complete connection ..." );
    /* should be waken by child process SIGUSR1 signal for real send */
    sleep(20);
    logp( LOGDBG , "father waiting time out" );
    logp( LOGDBG , "killing child process" );
    kill( g_childPid , SIGTERM );
    clean_send_proc();
    exit(0);
}
void real_send()
{
    logp( LOGDBG , "child connection finish , father starts to send");
    int turns = 0;
    int i = 0;
    int j = 0;
    struct timespec ts; 
    char buf[MAX_LINE_LEN];         
    char hex[MAX_LINE_LEN];         
    int msgLength = 0;
    int nTotal = 0;
    int nSent = 0;
    int nLeft = 0;
    unsigned int c = 0;
    /* calculate time interval */
    cal_time_ns( g_tps , &ts);
    /* set time last for alarm signal */
    if ( g_timeLast )
        alarm(g_timeLast);
    /* send */
    while (1){
        memset( hex , 0x00 , MAX_LINE_LEN);
        memset( buf , 0x00 , MAX_LINE_LEN);

        if ( fscanf( g_fp , "%s" , hex) == EOF ){
            fseek( g_fp , 0 , SEEK_SET);
            continue;
        }
        if ( g_hexMode == 1 ){
            i = 0;
            j = 0;
            while ( i < strlen(hex) ){
                sscanf( hex + i , "%02x" , &c );
                buf[j++] = (char)c;
                i += 2 ;
            }
        } else {
            memcpy( buf , hex , strlen(hex) );
        }
        /* validate length */
        msgLength = 0;
        for ( i = 0 ; i < 4 ; i ++)
            if ( buf[i] < '0' || buf[i] > '9'){
                logp( LOGERR , "invalid length");
                break;
            } else {
                msgLength *= 10;
                msgLength += (buf[i]-'0');
            }
        if ( msgLength == 0 )
            break;
        if ( ( g_hexMode == 1 && msgLength != strlen(hex)/2-4 ) || 
                ( g_hexMode == 0 && msgLength != strlen(buf)-4 ) ) {
            logp( LOGERR , "invalid length");
            break;
        }
        /* get a socket to send */
        persist_trac( LOGINF , 0 , msgLength + 4 , buf);
        nTotal = 0;
        nSent = 0;
        nLeft = msgLength + 4;
        while( nTotal != msgLength + 4){
            nSent = send( g_sock4send[turns] , buf + nTotal , nLeft , 0);
            if (nSent == 0){
                break;
            }
            else if (nSent < 0){
                logp( LOGERR , "sending socket fail");
                break;
            }
            nTotal += nSent;
            nLeft -= nSent;
        }
        turns = (turns + 1) % g_numOfSend;

        nanosleep(&ts , NULL);
    }
    clean_send_proc();
    exit(0);
}
void start_recv_proc()
{
    int rc = 0;
    int i = 0 ;
    int maxfdp = -1;
    fd_set fds;
    struct timeval timeout;
    char buffer[MAX_LINE_LEN];
    int recvlen = 0;
    int textlen = 0;
    int nTotal = 0;
    int nRead = 0;
    int nLeft = 0;
    for ( i = 0 ; i < g_numOfRecv ; i ++)
        maxfdp = maxfdp > g_sock4recv[i] ? maxfdp : g_sock4recv[i];
    maxfdp ++;
    while(1) {
        /* set time interval for select */
        timeout.tv_sec = 20;
        timeout.tv_usec = 0;
        FD_ZERO(&fds);
        for ( i = 0 ; i < g_numOfRecv ; i ++)
            FD_SET( g_sock4recv[i] ,  &fds );
        rc = select( maxfdp , &fds , NULL , NULL , &timeout );
        if ( rc < 0 ) {
            if ( errno == EINTR )
                continue;
            else
                break;
        } else if ( rc == 0 ) {
            /* select time out */
            logp( LOGDBG , "child select time out");
            break;
        } else {
            for ( i = 0 ; i < g_numOfRecv ; i ++) {
                if ( FD_ISSET(g_sock4recv[i] , &fds) ) {
                    memset(buffer , 0x00 , MAX_LINE_LEN);
                    recvlen = recv( g_sock4recv[i] , buffer , 4 , 0);
                    if (recvlen <= 0){
                        clean_recv_proc();
                        logp( LOGDBG , "child socket fail");
                        clean_recv_proc();
                        exit(0);
                    } else {
                        textlen = atoi(buffer);
                        nTotal = 0;
                        nRead = 0;
                        nLeft = textlen;
                        while(nTotal != textlen){
                            nRead = recv(g_sock4recv[i] , buffer + 4 + nTotal , nLeft , 0);
                            if (nRead == 0)    break;
                            nTotal += nRead;
                            nLeft -= nRead;
                        }
                    }
                    persist_trac( LOGINF , 1 , textlen+4 ,buffer);
                } /* end if */
            } /* end for */
        } /* end rc > 0 */ 
    } /* end while*/
    clean_recv_proc();
    exit(0);
}
void clean_send_proc()
{
    int i = 0;
    if ( g_fp != stdin )
        fclose(g_fp);
    for ( i = 0 ; i < g_numOfSend ; i ++)
        if ( g_sock4send[i] != 0 )
            close(g_sock4send[i]);
    return;
}
void clean_recv_proc()
{
    int i = 0;
    for ( i = 0 ; i < g_numOfRecv ; i ++)
        if ( g_sock4recv[i] != 0 )
            close(g_sock4recv[i]);
    return;
}
void father_signal_handler(int no)
{
    switch(no) {
        case SIGALRM :
            logp( LOGDBG , "father send over , wait for child to end");
            /* should be waken by SIGCHLD */
            sleep(20);
            logp( LOGDBG , "father wait timed out");
            clean_send_proc();
            exit(0);
        case SIGCHLD :
            logp( LOGDBG , "detect child time out , father quit");
            clean_send_proc();
            exit(0);
            break;
        case SIGUSR1 :
            real_send();
            break;
    }
    return ;
}
void cal_time_ns(int tps , struct timespec *ts)
{
    if (tps == 1){
        ts->tv_sec = 1;
        ts->tv_nsec = 0;
    } else {
        ts->tv_sec = 0;
        ts->tv_nsec = (long)((1.0/tps)*(1e9));
    }
    ts->tv_nsec *= g_percentage / 100;
    logp( LOGDBG , "interval[%d,%d]" , ts->tv_sec , ts->tv_nsec);
    return ;
}
void logp( log_level_t log_level , char *fmt , ...)
{
    if ( log_level > g_logLevel )   return;
    va_list ap;
    time_t t;
    struct tm *timeinfo;
    int sz = 0;
    struct timeval tv_now;
    char message[MAX_LINE_LEN];

    memset(message , 0x00 , MAX_LINE_LEN);
    t = time(NULL);
    gettimeofday(&tv_now , NULL);
    time(&t);
    timeinfo = localtime(&t);
    sz = sprintf(message , \
            "[%02d:%02d:%02d:%06d][PID<%-5d>][%6s][" ,\
            timeinfo->tm_hour , \
            timeinfo->tm_min, \
            timeinfo->tm_sec, \
            tv_now.tv_usec, \
            getpid(), \
            level_print[log_level] );
    va_start(ap , fmt);
    vsnprintf(message + sz , MAX_LINE_LEN - sz , fmt , ap);
    va_end(ap);

    fprintf(stdout , "%s]\n" , message);
    fflush(stdout);
    return ;
}
void persist_trac( log_level_t log_level , int flag , int msgLength , char *msg)
{
    if ( log_level > g_logLevel )   return;
    time_t t;
    struct tm *timeinfo;
    int sz = 0;
    struct timeval tv_now;
    char message[MAX_LINE_LEN];
    char *send_format = "[%02d:%02d:%02d:%06d][PID<%-5d>][%6s][SEND>";
    char *recv_format = "[%02d:%02d:%02d:%06d][PID<%-5d>][%6s][RECV>";

    memset(message , 0x00 , MAX_LINE_LEN);
    t = time(NULL);
    gettimeofday(&tv_now , NULL);
    time(&t);
    timeinfo = localtime(&t);
    sz = sprintf(message , \
            flag == 0 ? send_format : recv_format , \
            timeinfo->tm_hour , \
            timeinfo->tm_min, \
            timeinfo->tm_sec, \
            tv_now.tv_usec, \
            getpid(), \
            level_print[log_level] );
    if ( MAX_LINE_LEN - sz - 2 < msgLength ){
        memcpy( message + sz , msg , MAX_LINE_LEN - sz - 2);
        memcpy( message + MAX_LINE_LEN - 2 , "]\n" , 2);
        fwrite( message , sizeof(char) , MAX_LINE_LEN , stdout);
    } else {
        memcpy( message + sz , msg , msgLength);
        memcpy( message + sz + msgLength , "]\n" , 2);
        fwrite( message , sizeof(char) , sz + msgLength + 2 , stdout);
    }
    fflush(stdout);
    return ;

}
int main(int argc , char *argv[])
{
    int i = 0;
    int op = 0;
    int rc = 0;
    memset(g_inputFileName , 0x00 , FILENAME_LEN);
    strcpy(g_ipAddress , "127.0.0.1" );
    for ( i = 0 ; i < MAX_SOCK_NUM ; i ++) {
        g_sock4send[i] = 0;
        g_sock4recv[i] = 0;
        g_remotePorts[i] = 0;
        g_localPorts[i]  = 0;
    }
    g_numOfSend = 0;
    g_numOfRecv = 0;
    g_tps = 10;
    g_timeLast = 10;
    g_logLevel = LOGINF;
    g_percentage = 100.0;
    g_hexMode = 0;
    while ( ( op = getopt( argc , argv , "hHf:i:r:l:s:t:u:p:") ) > 0 ) {
        switch(op){
            case 'h' :
                print_help();
                exit(0);
            case 'f' :
                strncpy( g_inputFileName , optarg , FILENAME_LEN);
                break;
            case 'i' :
                memset( g_ipAddress , 0x00 , IP_LEN);
                strncpy( g_ipAddress , optarg , IP_LEN);
                break;
            case 'r' :
                parse_ports( optarg , g_remotePorts , &g_numOfSend);
                break;
            case 'l' :
                parse_ports( optarg , g_localPorts , &g_numOfRecv);
                break;
            case 's' :
                g_tps = atoi(optarg);
                if ( g_tps < 0 || g_tps > 20000 ) {
                    logp( LOGERR , "tps should between [1,200000]" );
                    exit(0);
                }
                break;
            case 't' :
                g_timeLast = atoi(optarg);
                if ( g_timeLast < 0 || g_timeLast > 999999 ) {
                    logp( LOGERR , "time should between [0,999999]" );
                    exit(0);
                }
                break;
            case 'u' :
                g_logLevel = atoi(optarg);
                if ( g_logLevel < 0 || g_logLevel > 6 ) {
                    logp( LOGERR , "level should between [0,6]" );
                    exit(0);
                }
                break;
            case 'p' :
                g_percentage = atoi(optarg);
                if ( g_percentage < 0 || g_percentage > 100 ) {
                    logp( LOGERR , "percentage should between [1,100]" );
                    exit(0);
                }
                break;
            case 'H' :
                g_hexMode = 1;
        }
    }
    logp(LOGDBG , "===========INPUT PARAMETERS=========");
    logp(LOGDBG , "g_inputFileName : [%s]" , g_inputFileName);
    logp(LOGDBG , "g_ipAddress     : [%s]" , g_ipAddress);
    logp(LOGDBG , "g_tps           : [%d]" , g_tps);
    logp(LOGDBG , "g_timeLast      : [%d]" , g_timeLast);
    logp(LOGDBG , "g_numOfSend     : [%d]" , g_numOfSend);
    logp(LOGDBG , "g_numOfRecv     : [%d]" , g_numOfRecv);
    for ( i  = 0 ; i < g_numOfSend ; i ++)
        logp(LOGDBG , "g_remotePorts[%d] : [%d]" , i , g_remotePorts[i] );
    for ( i  = 0 ; i < g_numOfRecv ; i ++)
        logp(LOGDBG , "g_localPorts[%d] : [%d]" , i , g_localPorts[i] );
    logp(LOGDBG , "===========INPUT PARAMETERS=========");
    if ( g_numOfSend == 0 ||  g_numOfRecv == 0 ){
        logp( LOGERR , "lack local or remote ports");
        print_help();
        exit(1);
    }
    int g_childPid = fork();
    if ( g_childPid < 0 ) {
        logp( LOGERR , "fork err");
        exit(1);
    } else if ( g_childPid == 0 ) {
        logp( LOGDBG , "child process fork OK , start to listen on local ports");
        rc = start_listen();
        if ( rc !=  0 ) {
            clean_recv_proc();
            exit(1);
        }
        /* tell father to start real send */
        kill( getppid() , SIGUSR1 );
        start_recv_proc();
    }
    start_send_proc();
    return 0;
}
