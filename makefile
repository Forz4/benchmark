linux: 
	@gcc -o benchmark -Wall -std=gnu99 benchmark.c
	@gcc -o Hex2Ascii -Wall -std=gnu99 Hex2Ascii.c

aix:	
	@cc -o benchmark benchmark.c
	@cc -o Hex2Ascii Hex2Ascii.c

clean:
	@rm -f *.o
	@rm -f benchmark Hex2Ascii

pack:
	@tar -cf benchmark.tar benchmark.c benchmark.h makefile Hex2Ascii.c 
