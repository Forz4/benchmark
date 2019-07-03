#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main( int argc , char *argv[] )
{
    int count = 0;
    char buf[2000];
    int i = 0;
    int j = 0;
    unsigned int n = 0;
    int flag = 0;    /* 0 for Hex to Ascii , 1 for Ascii to Hex */
    if ( argc > 1 )     flag = atoi(argv[1]);
    if ( argc > 2 )     count = atoi(argv[2]);

    memset( buf , 0x00 , 2000);
    while ( 1 ) {
        memset( buf , 0x00 , 2000);
        if ( count > 0 && j ++ >= count)                break;
        if ( fscanf( stdin , "%s" , buf) == EOF )  break;
        if ( flag == 0){
            i = 0;
            while ( i < strlen(buf) ){
                sscanf( buf+i , "%02x" , &n);
                printf( "%c" , (char)n);
                i += 2;
            }
        } else {
            for ( i = 0 ; i < strlen(buf)  ; i ++){
                fputc( '0'+buf[i]/16 , stdout);
                fputc( '0'+buf[i]%16 , stdout);
            }
        }
        fputc( '\n' , stdout);
    }
    return 0;
}
