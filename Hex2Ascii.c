#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main( int argc , char *argv[] )
{
    char hexArray[2000];
    int i = 0;
    unsigned int n = 0;
    memset( hexArray , 0x00 , 2000);
    while ( 1 ) {
        scanf( "%s" , hexArray);
        i = 0;
        while ( i < strlen(hexArray) ){
            sscanf( hexArray+i , "%02x" , &n);
            printf( "%c" , (char)n);
            i += 2;
        }
        fputc( '\n' , stdout);
        memset( hexArray , 0x00 , 2000);
    }
    return 0;
}
