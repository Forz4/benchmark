/* benchmark for TCPIP transactions by wanghao*/
#include "benchmark.h"
/* global variables */
FILE            *g_fp;                          /* input file pointer */
char            g_inputFileName[FILENAME_LEN];  /* intpu file name */
char            g_ipAddress[16];                /* remote IP*/
int             g_sock4send[MAX_SOCK_NUM];      /* sockets for sending */
int             g_numOfSend;                    /* number of sending sockets */
int             g_remotePorts[MAX_SOCK_NUM];    /* remote ports */
int             g_sock4recv[MAX_SOCK_NUM];      /* sockets for receiving */
int             g_localPorts[MAX_SOCK_NUM];     /* local ports */
int             g_numOfRecv;                    /* number of receiving sockets */
int             g_tps;                          /* transaction per second */
int             g_timeLast;                     /* total time last */
int             g_recvPid;                      /* recv pid */
log_level_t     g_logLevel;                     /* log level */
int             g_hexMode;                      /* hex mode */
int             g_fixInterval;                  /* fix interval*/
FILE            *g_fplog;                       /* for log printing */
int             g_MSGKEY;
int             g_qid;
/* prototypes */
void print_help();
void parse_ports(char *line , int ports[MAX_SOCK_NUM] , int *num);
int  start_listen();
void start_send_proc();
void real_send();
void send_signal_handler(int no);
void start_recv_proc();
void clean_send_proc();
void clean_recv_proc();
void logp(log_level_t log_level , char *fmt , ...);
int init_asyn();
int start_persist_proc();
/* functions */
void print_help()
{
    printf("benchmark\n");
    printf("HELP:\n");
    printf("    [-h]          print help page\n");
    printf("    [-f filename] to specify input file name , set to STDIN by default\n");
    printf("    [-H]          set input mode to Hex , default option is ascii\n");
    printf("    [-i ip]       remote IP address , set to 127.0.0.1 by default\n");
    printf("    [-r remote_port1,remote_port2,...]\n");
    printf("                  to specify remote port(s) , divided by comma\n");
    printf("    [-l local_port1,local_port2,...]\n");
    printf("                  to specify local listening port(s) , divided by comma\n");
    printf("    [-s tps]      transaction per second , valid from [10,100000] , set to 10 by default\n");
    printf("    [-t time]     to specify total time last , set to 10 by default , valid range [0,999999] , 0 means infinite time\n");
    printf("    [-u level]    to specify log level , valid range [0,6] , set to 4-LOGINF by default\n");
    printf("                  0-LOGNON , 1-LOGFAT , 2-LOGERR , 3-LOGWAN , 4-LOGINF , 5-LOGADT , 6-LOGDBG\n");
    printf("    [-p interval] to set fix interval time between transactions , valid from [0,100] us , default value is 10\n");
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
int init_asyn()
{
    int pid;
    g_qid = msgget( g_MSGKEY , IPC_CREAT|0600);
    if ( g_qid < 0 ){
        return -1;
    }
    logp( LOGDBG , "get qid[%d]" , g_qid);
    pid = fork();
    if ( pid == 0 ){
        fclose(g_fplog);
        g_fplog = NULL;
        start_persist_proc();
    } else if ( pid < 0 ){
        return -1;
    } 
    return 0;
}
int start_persist_proc()
{
    logp( LOGDBG , "into persist proc");
    int ret = 0;
    msg_st msgs;
    while (1) {
        alarm(0);
        signal(SIGALRM , SIG_DFL);
        alarm(20);
        memset( &msgs , 0x00 , sizeof(msg_st));
        ret = msgrcv( (key_t)g_qid , &msgs , sizeof(msg_st)-sizeof(long) , 0 , 0 );
        logp( LOGDBG , "get msg from queue[%s]" , msgs.text);
        if ( memcmp( msgs.text , "0000" , 4 ) == 0 ) break;
        logp( LOGINF , "<<<%s" , msgs.text);
    }
    if ( g_fplog != NULL )
        fclose(g_fplog);
    exit(0);
}
int start_listen()
{
    int i = 0;
    int server_sockfd;
    struct sockaddr_in server_sockaddr;
    struct sockaddr_in client_addr;
    socklen_t socket_len = sizeof(client_addr);
    for ( i = 0 ; i < g_numOfRecv ; i ++){
        logp( LOGDBG , "starts to listen on local_port[%d] ...",g_localPorts[i]);
        server_sockfd = socket(AF_INET , SOCK_STREAM , 0);
        /*bind*/
        server_sockaddr.sin_family = AF_INET;
        server_sockaddr.sin_port = htons(g_localPorts[i]);
        server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(server_sockfd , (struct sockaddr *)&server_sockaddr , sizeof(server_sockaddr)) == -1){
            logp( LOGERR, "bind err on port[%d]" , g_localPorts[i]);
            return -1;
        }
        /*listen*/
        if(listen(server_sockfd , 1) == -1){
            logp( LOGERR , "listen err on port[%d]" , g_localPorts[i]);
            return -1;
        }
        /*accept*/
        g_sock4recv[i] = accept(server_sockfd , (struct sockaddr *)&client_addr , &socket_len);
        if (g_sock4recv[i] < 0){
            logp( LOGERR , "accept err on port[%d]" , g_localPorts[i]);
            return -1;
        }
        logp( LOGDBG , "accept OK on port[%d]" , g_localPorts[i]);
    }
    return 0;
}
void start_send_proc()
{
    int i = 0;
    struct sockaddr_in servaddr;    
    int sock_send = 0; 
    /* handle signals */
    signal( SIGALRM , send_signal_handler);
    signal( SIGCHLD , send_signal_handler);
    signal( SIGUSR1 , send_signal_handler);
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
        logp( LOGDBG , "connected to remote port[%d]" ,  g_remotePorts[i]);
    }
    logp( LOGDBG , "start to wait 20s for RECEIVER to accept" );
    /* should be waken by recv process SIGUSR1 signal for real send */
    sleep(20);
    logp( LOGDBG , "waiting for RECEIVER timed out" );
    logp( LOGDBG , "killing RECEIVER process" );
    kill( g_recvPid , SIGTERM );
    clean_send_proc();
    exit(0);
}
void real_send()
{
    int     i = 0;
    int     j = 0;
    int     turns = 0;                  /* for choosing sockets */
    char    bufSend[MAX_LINE_LEN];     /* buffer for send */
    char    bufRead[MAX_LINE_LEN];     /* buffer for read*/
    int     msgLength = 0;
    int     nTotal = 0;
    int     nSent = 0;
    int     nLeft = 0;
    unsigned int c = 0;
    int     lineNbr = 0;
    char    *find = NULL;
    struct  timeval t_start;
    struct  timeval t_end;
    struct  timeval t_last;
    int     count = 0;
    int     intervalUs = 0;
    msg_st  msgs;
    int     rc = 0;

    memset( &t_start , 0x00 , sizeof(struct timeval) );
    memset( &t_end   , 0x00 , sizeof(struct timeval) );
    memset( &t_last  , 0x00 , sizeof(struct timeval) );
    if ( g_timeLast )
         alarm(g_timeLast);

    while (1){
        if ( count == 0 ){
            gettimeofday(&t_start, NULL);
        }
        memset( bufSend , 0x00 , MAX_LINE_LEN);
        memset( bufRead , 0x00 , MAX_LINE_LEN);
        if ( fgets( bufRead , MAX_LINE_LEN , g_fp) == NULL ){
            fseek( g_fp , 0 , SEEK_SET);
            lineNbr = 0;
            continue;
        }
        lineNbr ++;
        find = strchr( bufRead , '\n');
        if ( find != NULL ){
            *find = '\0';
        }
        if ( g_hexMode == 1 ){
            i = 0;
            j = 0;
            while ( i < strlen(bufRead) ){
                sscanf( bufRead + i , "%02x" , &c );
                bufSend[j++] = (char)c;
                i += 2 ;
            }
        } else {
            memcpy( bufSend , bufRead , strlen(bufRead) );
        }
        /* validate length */
        msgLength = 0;
        for ( i = 0 ; i < 4 ; i ++)
            if ( bufSend[i] < '0' || bufSend[i] > '9'){
                logp( LOGERR , "invalid first 4 bytes , linenbr[%d]" , lineNbr);
                break;
            } else {
                msgLength *= 10;
                msgLength += (bufSend[i]-'0');
            }
        if ( msgLength == 0 )
            break;
        if ( ( g_hexMode == 1 && msgLength != strlen(bufRead)/2-4 ) || 
                ( g_hexMode == 0 && msgLength != strlen(bufSend)-4 ) ) {
            logp( LOGERR , "invalid transaction length , lineNbr[%d] , bufRead[%s]" , lineNbr , bufRead);
            break;
        }
        if ( g_MSGKEY > 0 ){
            memset( &msgs , 0x00 , sizeof(msg_st));
            strcpy( msgs.text , bufRead);
            rc = msgsnd( (key_t)g_qid , &msgs , sizeof(msg_st)-sizeof(long) , 0);
            if ( rc < 0 ){
                logp( LOGDBG , "msgsnd fail , rc[%d]" , rc);
            }
        } else {
            logp( LOGINF , ">>>%s" , bufRead);
        }

        nTotal = 0;
        nSent = 0;
        nLeft = msgLength + 4;
        while( nTotal != msgLength + 4){
            nSent = send( g_sock4send[turns] , bufSend + nTotal , nLeft , 0);
            if (nSent == 0){
                break;
            }
            else if (nSent < 0){
                logp( LOGERR , "sending socket fail , remote port[%d]" , g_remotePorts[turns]);
                break;
            }
            nTotal += nSent;
            nLeft -= nSent;
        }
        turns = (turns + 1) % g_numOfSend;

        /* controlling tps */
        count = count + 1;
        if ( count < g_tps / DIVIDER ) {
            if ( g_fixInterval > 0 ){
                usleep( g_fixInterval );
            }
        } else if ( count == g_tps / DIVIDER ){
            count = 0;
            gettimeofday(&t_end, NULL);
            timersub(&t_end, &t_start, &t_last);
            intervalUs = 1000000 / DIVIDER - t_last.tv_usec;
            if ( intervalUs > 0 )
                usleep( intervalUs );
        }
    }
    clean_send_proc();
    exit(0);
}
void start_recv_proc()
{
    int         rc = 0;
    int         i = 0 ;
    int         j = 0;
    int         maxfdp = -1;
    fd_set      fds;
    struct      timeval timeout;
    char        buffer[MAX_LINE_LEN];
    char        hex[MAX_LINE_LEN];
    int         recvlen = 0;
    int         textlen = 0;
    int         nTotal = 0;
    int         nRead = 0;
    int         nLeft = 0;
    msg_st      msgs;
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
            logp( LOGDBG , "select time out");
            break;
        } else {
            for ( i = 0 ; i < g_numOfRecv ; i ++) {
                if ( FD_ISSET(g_sock4recv[i] , &fds) ) {
                    memset(buffer , 0x00 , MAX_LINE_LEN);
                    recvlen = recv( g_sock4recv[i] , buffer , 4 , 0);
                    if (recvlen <= 0){
                        logp( LOGDBG , "recv fail on port[%d]" , g_localPorts[i]);
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
                    if ( g_hexMode == 1){
                        memset( hex , 0x00 , MAX_LINE_LEN);
                        j = 0;
                        for ( i = 0 ; i < textlen+4 && j+1 < MAX_LINE_LEN ; i ++){
                            hex[j] = '0' + buffer[i] / 16;
                            hex[j+1] = '0' + buffer[i] % 16;
                            j += 2;
                        }
                    }
                    if ( g_MSGKEY > 0 ){
                        memset(&msgs , 0x00 , sizeof(msg_st));
                        if ( g_hexMode == 1){
                            strcpy(msgs.text , hex);
                        } else {
                            strcpy(msgs.text , buffer);
                        }
                        rc = msgsnd( (key_t)g_qid , &msgs , sizeof(msg_st)-sizeof(long) , 0);
                        if ( rc < 0 ){
                            logp( LOGDBG , "msgsnd fail , rc[%d]" , rc);
                        }
                    } else {
                        if ( g_hexMode == 1){
                            logp( LOGINF , "<<<%s" , hex);
                        } else {
                            logp( LOGINF , "<<<%s" , buffer);
                        }
                    }
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
    if ( g_fplog != NULL )
        fclose(g_fplog);
    return;
}
void clean_recv_proc()
{
    int i = 0;
    for ( i = 0 ; i < g_numOfRecv ; i ++)
        if ( g_sock4recv[i] != 0 )
            close(g_sock4recv[i]);
    if ( g_fplog != NULL )
        fclose(g_fplog);
    if ( g_MSGKEY > 0 ){
       msg_st msgs;
       memset( &msgs , 0x00 , sizeof(msg_st));
       strcpy(msgs.text , "0000");
       msgsnd( (key_t)g_qid , &msgs , sizeof(msg_st)-sizeof(long) , 0);
    }
    return;
}
void send_signal_handler(int no)
{
    switch(no) {
        case SIGALRM :
            logp( LOGDBG , "send time out , wait for RECEIVER to end");
            sleep(20);
            logp( LOGDBG , "wait for RECEIVER to end time out");
            clean_send_proc();
            exit(0);
        case SIGCHLD :
            logp( LOGDBG , "detect RECEIVER quit");
            clean_send_proc();
            exit(0);
            break;
        case SIGUSR1 :
            real_send();
            break;
    }
    return ;
}
void logp( log_level_t log_level , char *fmt , ...)
{
    if ( log_level > g_logLevel )   return;

    char logFileName[FILENAME_LEN];
    va_list ap;
    time_t t;
    struct tm *timeinfo;
    int sz = 0;
    struct timeval tv_now;
    char message[MAX_LINE_LEN];

    if ( g_fplog == NULL ){
        sprintf( logFileName , "%d.log" , getpid());
        g_fplog = fopen( logFileName , "a+");
        if ( g_fplog == NULL){
            g_fplog = stdout;
        }
    }

    memset(message , 0x00 , MAX_LINE_LEN);
    t = time(NULL);
    gettimeofday(&tv_now , NULL);
    timeinfo = localtime(&t);
    sz = sprintf(message , \
            "[%02d:%02d:%02d:%06d][PID<%-10d>][%6s][" ,\
            timeinfo->tm_hour , \
            timeinfo->tm_min, \
            timeinfo->tm_sec, \
            tv_now.tv_usec, \
            getpid(), \
            level_print[log_level] );
    va_start(ap , fmt);
    vsnprintf(message + sz , MAX_LINE_LEN - sz , fmt , ap);
    va_end(ap);

    fprintf( g_fplog , "%s]\n" , message);
    fflush( g_fplog );
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
    g_hexMode = 0;
    g_fixInterval = 10;
    g_MSGKEY = 0;
    while ( ( op = getopt( argc , argv , "hHf:i:r:l:s:t:u:p:q:") ) > 0 ) {
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
                g_tps -= g_tps % DIVIDER;
                if ( g_tps < 10 || g_tps > 100000 ) {
                    logp( LOGERR , "tps should between [10,100000]" );
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
                g_fixInterval = atoi(optarg);
                if ( g_fixInterval < 0 || g_fixInterval > 100 ) {
                    logp( LOGERR , "interval should between [0,100]" );
                    exit(0);
                }
                break;
            case 'H' :
                g_hexMode = 1;
                break;
            case 'q' :
                g_MSGKEY = atoi(optarg);
                break;
        }
    }
    logp(LOGDBG , "===========INPUT PARAMETERS=========");
    logp(LOGDBG , "g_inputFileName : [%s]" , g_inputFileName);
    logp(LOGDBG , "g_ipAddress     : [%s]" , g_ipAddress);
    logp(LOGDBG , "g_tps           : [%d]" , g_tps);
    logp(LOGDBG , "g_timeLast      : [%d]" , g_timeLast);
    logp(LOGDBG , "g_numOfSend     : [%d]" , g_numOfSend);
    logp(LOGDBG , "g_numOfRecv     : [%d]" , g_numOfRecv);
    logp(LOGDBG , "g_MSGKEY        : [%d]" , g_MSGKEY);
    logp(LOGDBG , "g_hexMode       : [%d]" , g_hexMode);
    logp(LOGDBG , "g_logLevel      : [%d]" , g_logLevel);
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
    int g_recvPid = fork();
    if ( g_recvPid < 0 ) {
        logp( LOGERR , "fork err");
        exit(1);
    } else if ( g_recvPid == 0 ) {
        g_fplog = NULL;
        if ( g_MSGKEY > 0 ){
            logp( LOGDBG , "start to init asyn queue");
            rc = init_asyn();
            if ( rc < 0 ){
                clean_recv_proc();
                exit(1);
            }
        }
        logp( LOGDBG , "start to listen on local ports");
        rc = start_listen();
        if ( rc !=  0 ) {
            clean_recv_proc();
            exit(1);
        }
        /* tell send to start real send */
        kill( getppid() , SIGUSR1 );
        start_recv_proc();
    }
    start_send_proc();
    return 0;
}
